/***************************************************************************
                          logger.h  -  description
                             -------------------
    version              : 0.08
    begin                : Thu Sep 11 2003
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

#ifndef LOGGER_H
#define LOGGER_H
#define LOGGER_H_VERSION 8 // Meaning 0.08

#include <string>
#include "check_library_versions.h" // Always last: Check the versions of included libraries.

using namespace std;

/**
  *@author David Purdy
  */

// Logging (errors, messages, warnings)
enum log_type {
  LT_MESSAGE,
  LT_WARNING,
  LT_ERROR
};

const log_type default_log_type=LT_MESSAGE;

/****************************************
  base logger class
****************************************/

// This is a "pure virtual" class to base other log-writing functions on.
class logger {
public:
  logger(); // Constructor
  virtual ~logger() {} // A virtual destructor to remove compile warnings...

  // This function is called by an outside processes when there is a log to be handled is picked up.
  void set_log(const string & strMessage_="",    // eg: "Could not find the config file! Terminating"
                      const log_type LogType_=default_log_type,            // eg: LT_ERROR
                      const string & strProg_="",           // eg: "player"
                      const string & strSourceFile_="", // eg: "player.cpp"
                      const string & strSourceFunction_="", // eg: "player_init_func"
                      const long lngSourceLine_=0); // eg: 241

   virtual void handle_log() = 0; // This function is defined in child classes - the specific handling happens
                                                 // here.

    // And now methods for retrieving a line describing the log, and also methods for customising the line
    // contents.
    //
   string GetLogDescr(const string & strFormat = "%DEFAULT"); // Return a single line describing the log.

   // Formatting information for the logs:
   void SetDateFormat(const string & strdate_format) { strDateFormat = strdate_format; } // eg: "%dd/%mm/%yyyy"
   void SetTimeFormat(const string & strtime_format) { strTimeFormat = strtime_format; } // eg: "%HH:%NN:%SS"
   void SetLogFormat(const string & strlog_format) { strLogFormat = strlog_format; } // eg: "[%DATE %TIME] %FUNCTION(): %TYPE_NOTDEFAULT%MESSAGE"

   // Strings passed to SetLogFormat() can include special strings which will be replaced with log info:   

   //  %DATE - Date of the log
   //  %TIME - Time of the log
   //  %SOURCE_ERROR - If the message type is ERROR, then include " (%FILE:%LINE)"
   //  %PROGRAM - Program that made the log
   //  %FILE    - Source file where the log occured
   //  %FUNCTION - Function where the log occured
   //  %LINE - Line number where the log occured
   //  %TYPE_NOTDEFAULT - If the message type is not default, then include "%TYPE: "
   //      - ie, don't show the log type if the log is a normal message (not a warning or error)
   //  %TYPE - The type of the log: MESSAGE/WARNING/ERROR
   //  %MESSAGE - The message that is being logged.

protected:
  // The details of the log:
  struct log_info  {
    string strMessage;         // eg: "Could not find the config file! Terminating"
    log_type LogType;          // eg: ERROR
    string strProg;            // eg: "player"
    string strSourceFile;      // eg: "player.cpp"
    string strSourceFunction;  // eg: "player_init_func"
    long lngSourceLine;        // eg: 241
  } log_info;

  // Functions to get descriptions of the enumerations... LogType
  string GetLogTypeDescr();

private:
  // Format of the log:
 string strDateFormat; 
 string strTimeFormat;
 string strLogFormat;
};



#endif
