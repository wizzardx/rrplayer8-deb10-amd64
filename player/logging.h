/***************************************************************************
                          logger.h  -  description
                             -------------------
    version              : 0.08
    begin                : Mon Sep 8 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef LOGGING_H
#define LOGGING_H
#define LOGGING_H_VERSION 8 // Meaning 0.08

#include <string>
#include "config.h"

#include "logger.h" // for the logger class
#include "check_library_versions.h" // Always last: Check the versions of included libraries.

/**
  *@author David Purdy
  */

///*********************************************************************************************************
//  And now a global object for the entire program: For managing application logging
//**********************************************************************************************************/

// The default logging class (used internally by the logging class)
class default_logger : public logger {
public:
  default_logger();  // Constructor
  virtual void handle_log(); // Handle log operations.
  void set_logfile_path(const string & strLogFilePath) { strlog_file = strLogFilePath; } // update the log path used by handle_log
  void write_line(const string &strLine); // Write a line to the logfile

  // Screen output format:
  void set_screen_log_format(const string strscreen_log_format) { strScreenLogFormat = strscreen_log_format; }  // If this function is called, the log info sent to the screen will be different to the logfile

  // Enable/Disable automatic log rotation (eg: you want to use linux's built-in logrotate instead.
  void enable_logrotate(const bool blnrotate) { blnlogrotate_enabled = blnrotate; }
  
private:
  // The log file:
  string strlog_file;

  // Logfile rotation:
  bool blnlogrotate_enabled; // Set to false if automatic log rotation is disabled;
  bool is_time_to_rotate_logfile();
  void rotate_logfile()  ;

  string strScreenLogFormat; // If this is set, the log output sent to the screen will be different to the log output sent to the logfile.
};

// The global logging class:

class logging {
public:
  // The constructor
  logging();

  // Allow the user to set a Custom logger object
  void set_custom_logger(logger * Logger); // Client programs can create their own logger handlers.

  // This function is called by the macros to do logging to the default and the custom logger objects:
  void do_log(const string & strMessage="", const log_type LogType=default_log_type, const string & strProg="", const string & strSourceFile="", const string & strSourceFunction="", const long lngSourceLine=0);

  default_logger default_log;
  
private:
  // Private data:
  logger * CustomLogger; // This is the user's logging object.
  bool blnin_do_log; // Set when the object is within "do_log" - the function is not allowed to indirectly
                     // call itself, whether through an error in the default logger (text file + screen), or through
                     // a custom logger! (eg: database)
};

// And the global logging object:
extern logging Logging; // this is the Logging object you use in your client code...

///*********************************************************************************************************
//  Macros to do the grunt work: Call these throughout your programs:
//**********************************************************************************************************/

// log_line just writes a log to the default logs (clog and the textfile) - this is for places where
// you want to decorate your logs but don't want to decorate your custom logs (eg a database)
#define log_line(strMessage) Logging.default_log.set_log(strMessage, LT_MESSAGE, PACKAGE, __FILE__, __FUNCTION__, __LINE__); Logging.default_log.handle_log()

// These macros will log to the default logs and to the custom logging object (if you have defined one). See logging.h
#define log_message(strMessage) Logging.do_log(strMessage, LT_MESSAGE, PACKAGE, __FILE__, __FUNCTION__, __LINE__)
#define log_warning(strMessage) Logging.do_log(strMessage, LT_WARNING, PACKAGE, __FILE__, __FUNCTION__, __LINE__)
#define log_error(strMessage) Logging.do_log(strMessage, LT_ERROR, PACKAGE,  __FILE__, __FUNCTION__, __LINE__)

#endif
