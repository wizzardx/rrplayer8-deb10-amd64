/***************************************************************************
                          logging.cpp  -  description
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

#include "logging.h"
#include "rr_utils.h"
#include <stdio.h>
#include <sys/stat.h>
#include <iostream>
#include "check_library_versions.h"

using namespace rr;

/****************************************
    default_logger class
****************************************/

default_logger::default_logger() { // Constructor
  strlog_file = GetExecDir() + PACKAGE + ".log";
  strScreenLogFormat = ""; // The format of the screen output (if this is set, it will be different to the logfile output)
  blnlogrotate_enabled = true; // Set to false
}

// The handling method for the default handling:
void default_logger::handle_log() {
  // Fetch the log description:
  string strLine=GetLogDescr();

  // Log to clog
  if (strScreenLogFormat != "") {
    // A special format is defined for the screen output:
    clog << GetLogDescr(strScreenLogFormat) << endl;
  }
  else {
    // Use the same log format for the screen & the logfile.
    clog << strLine << endl;      
  }

  // Log to the logfile

  // Check if it is time to rotate the logfile (ie, has the next day arrived.
  if (is_time_to_rotate_logfile()) {
    rotate_logfile();
  } // end if
    
  AppendStringToTextFile(strLine, strlog_file); // Throws an exception if there is an error
}

// Write a line to the logfile
void default_logger::write_line(const string &strLine) {
  // Write a plain line, not including the time, location, etc, etc
  try {
    // Log to clog
    clog << strLine << endl;

    // Log to the logfile
    try {
      AppendStringToTextFile(strLine, strlog_file); // Logs & throws an exception if there is an error.
    } // Catch any exceptions that take place in AppendStringToTextFile()
    catch(rr_exception & E) {
      cerr << "Error while writing to logfile: " << E.what() << endl;
    }
    catch(...) {
      cerr << "Unknown error while writing to the logfile! (" << __FILE__ << ":" << __LINE__ << ")" << endl;
    }
  }
  catch(...) { } // Don't do anything if there are uncaught exceptions in write_line!!
}

// - logfile rotation
bool default_logger::is_time_to_rotate_logfile() {
#ifndef __linux__
  undefined_func;
  rr_throw("Function only defined for linux...");
#else

  // Is rotation disabled?
  if (!blnlogrotate_enabled) return false;

  bool blnRotate = true; // assume that the logfile must be rotated

  // Check the logfile size. If it is too large ( > 50 MB) then delete it! The logs cannot be allowed to grow so large.

  // The only time we don't rotate the logfile, is
  //  1) The logfile does not exist
  //  2) The .rotated file's date is not today.
  // Logfile exists?
  if (FileExists(strlog_file)) {
    // Check logfile size (kill it if it is > 50 MB)
    long lngMaxSize = 50 * 1024 * 1024;
    if (FileSize(strlog_file) <= lngMaxSize) {
      // Logfile is not too large.

      // Now check the "log_dir"/logs/ subdirectory, and see if there is a .rotated file
      // 1) Get the path component of the logfile path.
      string strLogDir, strLogFName;
      break_down_file_path(strlog_file, strLogDir, strLogFName);

      // 2) Build the "logs" sub-directory path
      string strLogBackupDir = strLogDir + "./logs/";

      // 3) Create the directory if it isn't found
      if (!DirExists(strLogBackupDir)) {
        mkdir(strLogBackupDir); // Throws an exception on failure
      }

      // 4) Check if the directory exists now.
      if (DirExists(strLogBackupDir)) {
        // ./logs/ subdirectory exists now.
        // 4) Check for the existance of a ".rotated" file
        string strRotateFile = strLogBackupDir + ".rotated";

        if (FileExists(strRotateFile)) {
          // 5) Check if the date has changed since the modification date of the .rotated file.
          if (GetDateTimeDate(GetFileDateModified(strRotateFile)) == Date()) {
            // Date has not changed - don't rotate
            blnRotate = false;
          }
        }
        else {
          // File not found - don't rotate, but also create the .rotated file
          blnRotate = false;
          AppendStringToTextFile("", strRotateFile); // Logs an error and throws an exception if there are errors.
        }
      }
      else {
        // log backup directory could not be created!
        write_line(string(__FUNCTION__) + "(): Could not create the log backup directory!");
      }
    }
    else {
      // Logfile is too big! Kill it
      write_line(string(__FUNCTION__) + "(): ERROR: Logfile is larger than 50MB!");
      remove(strlog_file.c_str());
      write_line(string(__FUNCTION__) + "(): Logfile was too large so it was removed");
      blnRotate = false;
    }
  } // end if
  else {
    // File does not exist - so don't rotate it.
    blnRotate = false;
  }
  return blnRotate;
#endif  
}

void default_logger::rotate_logfile() {
#ifndef __linux__
  undefined_func;
  rr_throw("Function only defined for linux...");
#else
  write_line(string(__FUNCTION__) + "(): rotating logfile...");
  // Logfile exists?
  if (FileExists(strlog_file)) {
    // 1) Get the path component of the logfile path.
    string strLogDir, strLogFName;
    break_down_file_path(strlog_file, strLogDir, strLogFName);

    // 2) Build the "logs" sub-directory path
    strLogDir=ensure_char_at_end(strLogDir, '/');
    string strLogBackupDir = strLogDir + "logs/";

    // 3) Create the directory if it isn't found
    if (!DirExists(strLogBackupDir)) {
      mkdir(strLogBackupDir); // Throws an exception if there is an error.
    } // end if

    // 4) Check if the directory exists now.
    if (DirExists(strLogBackupDir)) {
      // ./logs/ subdirectory exists now.

      // Create a tar file in the logs subdirectory
      //   -- generate the filename
      int Year, Month, Day;
      DateTime dtmNow = Now();
      dtmNow -= 60*60*24; // Subtract a day from the current time - we want the log filename
                                       // to reflect the day passed, not the new day
      GetDateParts(dtmNow, Year, Month, Day);

      char strDay[3];
      sprintf(strDay, "%02d", Day);
      string strBackupFile  = strLogBackupDir + string(PACKAGE) + "_log_" + strDay; // no .tar or .gz at the end

      // create the tar (kill any existing tars of the same name)
      if (system(string("tar -cf " + strBackupFile + ".tar " + strlog_file).c_str()) == 0) {
        // Tar operation was successful.

        // Gzip the tar (overwrite any gzips with the same name)
        if (system(string("gzip -f " + strBackupFile + ".tar").c_str()) == 0) {
          // The gzip instruction succeeded
          // Reset the old logfile
          remove(strlog_file.c_str());

          // Reset the .rotated file, this lets us know when to perform the rotation again...
          string strRotateFile = strLogBackupDir + ".rotated";
          remove(strRotateFile.c_str());
          AppendStringToTextFile("", strRotateFile); // Logs an error and throws an exception if there are any errors.
          // Now write the opening log message...
          write_line("Previous logfile was archived to " + strBackupFile + ".tar.gz");
        }
        else {
          // The gzip instruction failed!
          write_line(string(__FUNCTION__) + "(): The gzip command failed!");
        }
      }
      else {
        // tar operation failed!
        write_line(string(__FUNCTION__) + "(): The tar command failed!");
      }
    } // end if (DirExists(strLogBackupDir))
    else {
      // log backup directory could not be created!
      write_line(string(__FUNCTION__) + "(): Could not find the log backup subdirectory!");
    } // end else
  } // if (FileExists(strLogFile))
#endif  
}

/*********************************************************
   logging class - the global logging manager:
**********************************************************/

// The global object that client programs can use to configure their logging.
logging Logging;

// The body of the logging class..

// The constructor
logging::logging()  {
  blnin_do_log=false; // We're not already in the "do_log" function
}

// Allow the user to set a Custom logger object
void logging::set_custom_logger(logger * Logger) {
  // Client programs can create their own logger handlers.
  CustomLogger=Logger;
}

// This function is called by the macros to do logging:
void logging::do_log(const string & strMessage, const log_type LogType, const string & strProg, const string & strSourceFile, const string & strSourceFunction, const long lngSourceLine) {
  try {
    // Is this function already being called?
    if (blnin_do_log) {
      default_log.write_line("WARNING: While logging a message, an attempt was made to log a second message: " + strMessage + " (" + strSourceFile + ":" + itostr(lngSourceLine) + ")");            
      return;
    }

    // Now we are officially within do_log:
    blnin_do_log = true; // Block other calls to do_log until this function is finished.
    
    // Do the default logging:
    default_log.set_log(strMessage, LogType, strProg, strSourceFile, strSourceFunction, lngSourceLine);
    default_log.handle_log();



    // Do the custom logging (if a custom logger is defined)
    if (CustomLogger != NULL) {
      CustomLogger->set_log(strMessage, LogType, strProg, strSourceFile, strSourceFunction, lngSourceLine);
      CustomLogger->handle_log();
    }
  } // END: try
  catch (const rr_exception & E) {
    default_log.write_line("Error occured during logging: " + (string)E.what());
  }
  catch(...) {
    default_log.write_line("An unexpected exception was thrown while logging!");
  }

  // The call to do_log is now finished, enable later calls:
  blnin_do_log = false;
}
