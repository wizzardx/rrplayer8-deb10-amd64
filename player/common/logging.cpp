#include "logging.h"
#include <iostream>
#include "testing.h"
#include "my_string.h"
#include "my_time.h"
#include "file.h"
#include <config.h> // For the PACKAGE variable.

// The global object used for logging.
clogging logging;

// Add a callback function for logging.
void clogging::add_logger(void(func)(const log_info&)) {
  callbacks.push_front(func);
}

void clogging::log(const log_type LT, const string & strdesc, const string & strfile, const string & strfunc, const int & intline) {
  // Setup the structure with logging info:
  log_info L = {LT, strdesc, strfile, strfunc, intline};

  try {
    // Don't allow this function to be called recursively:
    static bool blnrunning = false;
    if (blnrunning) {
      testing;  
      cerr << "*** The logging functions are not re-entrant! Check your logic!" << endl;
      cerr << "*** You tried to log this message during your log handler:" << format_log(L, strstandard_log_format) << endl;
      cerr << "***   " << format_log(L, strstandard_log_format) << endl;
      testing;  
    }
    blnrunning = true;

    // Call all the callbacks, with the logging info
    callback_slist::const_iterator I = callbacks.begin();
    while (I != callbacks.end()) {
      try {
        (*I)(L);
      }
      catch (const exception & e) {
        cerr << "*** An error occured while logging!" << endl;
        cerr << "*** Here is the error: " << e.what() << endl;
        cerr << "*** The following was being logged: " << format_log(L, strstandard_log_format) << endl;
      }
      catch (...) {
        testing;
        cerr << "*** An unidentifiable error occured while logging!" << endl;
        cerr << "*** The following was being logged: " << format_log(L, strstandard_log_format) << endl;
        testing;
      }
      ++I;
    }
    blnrunning = false;
  } catch(...) {
    testing;
    cerr << "*** An unexpected exception was thrown in " << __FUNCTION__<< "!" << endl;
    testing;
  }
  };

string format_log(const log_info & log_info, const string strformat) {
  // Get a string describing the log type:
  string strlog_type = "";
  switch(log_info.LT) {
    case LT_LINE:    strlog_type="LINE";    break;
    case LT_MESSAGE: strlog_type="MESSAGE"; break;
    case LT_WARNING: strlog_type="WARNING"; break;
    case LT_ERROR:   strlog_type="ERROR";   break;
    default: my_throw("Logic Error!");
  }

  string strret = strformat;
  strret=replace(strret, "%DEFAULT", strformat); // %DEFAULT is for when the user does not specify a format.
  strret=replace(strret, "%SOURCE_ERROR", ((log_info.LT == LT_ERROR) ? " (%FILE:%LINE)": ""));
  strret=replace(strret, "%DATE", format_datetime(now(), "%F"));
  strret=replace(strret, "%TIME", format_datetime(now(), "%T"));
  strret=replace(strret, "%FILE", log_info.strfile);
  strret=replace(strret, "%FUNCTION", log_info.strfunc);
  strret=replace(strret, "%LINE", itostr(log_info.intline));

  // %TYPE_NOT_LOG - Show the log type if it is a warning or error (ie, not a regular log message)
  strret=replace(strret, "%TYPE_NOT_LOG", ((log_info.LT != LT_LINE && log_info.LT != LT_MESSAGE) ? "%TYPE: ": ""));
  strret=replace(strret, "%TYPE", strlog_type);
  strret=replace(strret, "%MESSAGE", log_info.strdesc);

  return strret;
}


bool is_time_to_rotate_logfile(const string & strlog_file) {
  // A function called from rotate_logfile. Time to do a logfile rotation?
#ifndef __linux__
  my_throw("Function only defined for linux...");
#else
  bool blnrotate = true; // assume that the logfile must be rotated

  // Check the logfile size. If it is too large ( > 50 MB) then delete it! The logs cannot be allowed to grow so large.

  // The only time we don't rotate the logfile, is
  //  1) The logfile does not exist
  //  2) The .rotated file's date is not today.
  // Logfile exists?
  if (file_exists(strlog_file)) {
    // Check logfile size (kill it if it is > 50 MB)
    long lngMaxSize = 50 * 1024 * 1024;
    if (file_size(strlog_file) <= lngMaxSize) {
      // Logfile is not too large.

      // Now check the "log_dir"/logs/ subdirectory, and see if there is a .rotated file
      // 1) Get the path component of the logfile path.
      string strLogDir, strLogFName;
      break_down_file_path(strlog_file, strLogDir, strLogFName);

      // 2) Build the "logs" sub-directory path
      string strLogBackupDir = strLogDir + "logs/";

      // 3) Create the directory if it isn't found
      if (!dir_exists(strLogBackupDir)) {
        mkdir(strLogBackupDir); // Throws an exception on failure
      }

      // 4) Check if the directory exists now.
      if (dir_exists(strLogBackupDir)) {
        // ./logs/ subdirectory exists now.
        // 4) Check for the existance of a ".rotated" file
        string strRotateFile = strLogBackupDir + ".rotated";

        if (file_exists(strRotateFile)) {
          // 5) Check if the date has changed since the modification date of the .rotated file.
          if (get_datetime_date(get_file_date_modified(strRotateFile)) == date()) {
            // Date has not changed - don't rotate
            blnrotate = false;
          }
        }
        else {
          // File not found - don't rotate, but also create the .rotated file
          blnrotate = false;
          append_file_str(strRotateFile, ""); // Logs an error and throws an exception if there are errors.
        }
      }
      else {
        testing;
        // log backup directory could not be created!
        string strout = (string) "*** " + __FUNCTION__ + "(): Could not create the log backup directory!";
        append_file_str(strlog_file, strout);
        cerr << strout << endl;
        testing;
      }
    }
    else {
      testing;
      // Logfile is too big! Kill it
      rm(strlog_file.c_str());
      string strout = (string) "*** " + __FUNCTION__ + "(): ERROR: Logfile was larger than 50MB! It was removed!";
      append_file_str(strlog_file, strout);
      cerr << strout << endl;
      blnrotate = false;
      testing;
    }
  } // end if
  else {
    testing;
    // File does not exist - so don't rotate it.
    blnrotate = false;
    testing;
  }
  return blnrotate;
#endif
}

void rotate_logfile(const string & strlog_file) {
  // A function you can call to do simple day of month-based log rotation. compressed log files
  // are stored under a "logs" sub-directory.

  if (!is_time_to_rotate_logfile(strlog_file)) return;

#ifndef __linux__
  undefined_func;
  rr_throw("Function only defined for linux...");
#else
  string strout = string(__FUNCTION__) + "(): rotating logfile...";
  append_file_str(strlog_file, strout);
  clog << strout << endl;

  // Logfile exists?
  if (file_exists(strlog_file)) {
    testing;
    // 1) Get the path component of the logfile path.
    string strLogDir, strLogFName;
    break_down_file_path(strlog_file, strLogDir, strLogFName);

    // 2) Build the "logs" sub-directory path
    strLogDir=ensure_last_char(strLogDir, '/');
    string strLogBackupDir = strLogDir + "logs/";

    // 3) Create the directory if it isn't found
    if (!dir_exists(strLogBackupDir)) {
      testing;
      mkdir(strLogBackupDir); // Throws an exception if there is an error.
      testing;
    } // end if

    // 4) Check if the directory exists now.
    if (dir_exists(strLogBackupDir)) {
      testing;
      // ./logs/ subdirectory exists now.

      // Create a tar file in the logs subdirectory
      //   -- generate the filename
      int Year, Month, Day;
      datetime dtmNow = now();
      dtmNow -= 60*60*24; // Subtract a day from the current time - we want the log filename
                                       // to reflect the day passed, not the new day
      get_date_parts(dtmNow, Year, Month, Day);

      char strDay[3];
      sprintf(strDay, "%02d", Day);
      string strBackupFile  = strLogBackupDir + string(PACKAGE) + "_log_" + strDay; // no .tar or .gz at the end

      // create the tar (kill any existing tars of the same name)
      if (system(string("tar -cf " + strBackupFile + ".tar " + strlog_file).c_str()) == 0) {
        testing;
        // Tar operation was successful.

        // Gzip the tar (overwrite any gzips with the same name)
        if (system(string("gzip -f " + strBackupFile + ".tar").c_str()) == 0) {
          testing;
          // The gzip instruction succeeded
          // Reset the old logfile
          remove(strlog_file.c_str());

          // Reset the .rotated file, this lets us know when to perform the rotation again...
          string strRotateFile = strLogBackupDir + ".rotated";
          remove(strRotateFile.c_str());
          append_file_str(strRotateFile, ""); // Logs an error and throws an exception if there are any errors.
          // Now write the opening log message...
          string strout = "Previous logfile was archived to " + strBackupFile + ".tar.gz";
          append_file_str(strlog_file, strout);
          clog << strout << endl;
          testing;
        }
        else {
          testing;
          // The gzip instruction failed!
          string strout = (string) "*** " + __FUNCTION__ + "(): The gzip command failed!";
          append_file_str(strlog_file, strout);
          cerr << strout << endl;
          testing;
        }
        testing;
      }
      else {
        testing;
        // tar operation failed!
        string strout = (string) "*** " + __FUNCTION__ + "(): The tar command failed!";
        append_file_str(strlog_file, strout);
        cerr << strout << endl;
        testing;
      }
    } // end if (DirExists(strLogBackupDir))
    else {
      testing;
      // log backup directory could not be created!
      string strout = (string) "*** " + __FUNCTION__ + "(): Could not find the log backup subdirectory!";
      append_file_str(strlog_file, strout);
      cerr << strout << endl;
      testing;
    } // end else
    testing;
  } // if (FileExists(strLogFile))
  testing;
#endif
}
