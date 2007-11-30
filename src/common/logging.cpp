#include "logging.h"
#include <iostream>
#include "my_string.h"
#include "my_time.h"
#include "file.h"
#include "system.h"
#include "dir_list.h"
#include "exception.h"

#ifndef __linux__
  #include "testing.h"
#endif

// The global object used for logging.
clogging logging;

// clogging constructor
clogging::clogging() {
  blndebug = false;
}

// Add a callback function for logging.
void clogging::add_logger(void(*func)(const log_info&)) {
  callbacks.push_front(func);
}

void clogging::log(const log_type LT, const string & strdesc, const string & strfile, const string & strfunc, const int & intline) {
  // Setup the structure with logging info:
  log_info L = {LT, strdesc, get_short_filename(strfile), strfunc, intline};

  // Don't allow this function to be called recursively:
  static bool blnrunning = false;

  try {
    if (blnrunning) {
      cerr << "*** The logging functions are not re-entrant! Check your logic!" << endl;
      cerr << "*** You tried to log this message during your log handler:" << endl;
      cerr << "***   " << format_log(L, strstandard_log_format) << endl;
      return;
    }
    blnrunning = true;

    // If we don't have any callbacks, then log to stdout:
    if (callbacks.size() == 0) cout << format_log(L, strstandard_log_format) << endl;

    // Call all the callbacks, with the logging info
    callback_slist::const_iterator I = callbacks.begin();

    // Loop through the callbacks:
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
        cerr << "*** An unknown exception was thrown while logging!" << endl;
        cerr << "*** The following was being logged: " << format_log(L, strstandard_log_format) << endl;
      }
      ++I;
    }
  } catch(...) {
    cerr << "*** An unexpected exception was thrown in " << __FUNCTION__<< "()!" << endl;
  }

  blnrunning = false;
};

string format_log(const log_info & log_info, const string strformat) {
  // Get a string describing the log type:
  string strlog_type = "";
  switch(log_info.LT) {
    case LT_LINE:    strlog_type="LINE";    break;
    case LT_DEBUG:   strlog_type="DEBUG"; break;
    case LT_MESSAGE: strlog_type="MESSAGE"; break;
    case LT_WARNING: strlog_type="WARNING"; break;
    case LT_ERROR:   strlog_type="ERROR";   break;
    default: LOGIC_ERROR;
  }

  // Get a string representation of the time:
  string strtime = format_datetime(now(), "%H:%M:%S");
  // Get a string representation of the time, including milliseconds:
  string strtime_ms;
  {
    timeval tvnow;
    gettimeofday(&tvnow, NULL);
    strtime_ms = strtime + "," + pad_left(itostr(tvnow.tv_usec / 1000), '0', 3);
  }

  // Now perform log string formatting:
  string strret = strformat;
  strret=replace(strret, "%DEFAULT", strformat); // %DEFAULT is for when the user does not specify a format.
  strret=replace(strret, "%SOURCE_ERROR", ((log_info.LT == LT_ERROR) ? " (%FILE:%LINE)": ""));
  strret=replace(strret, "%DATE", format_datetime(now(), "%Y-%m-%d"));
  strret=replace(strret, "%TIME_MS", strtime_ms);
  strret=replace(strret, "%TIME", strtime);
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

  // Check the logfile size. If it is too large ( > 50 MB) then delete it! The logs cannot be allowed to grow so large.

  // The only time we don't rotate the logfile, is
  //  1) The logfile does not exist
  //  2) The .rotated file's date is not today.

  // Logfile exists?
  if (!file_exists(strlog_file)) return false; // Don't rotate if not found.

  // Check logfile size (kill it if it is > 50 MB)
  long lngMaxSize = 50 * 1024 * 1024;
  if (file_size(strlog_file) > lngMaxSize) {
    // Logfile is too big! Kill it
    rm(strlog_file.c_str());
    string strout = (string) "*** " + __FUNCTION__ + "(): ERROR: Logfile was larger than 50MB! It was removed!";
    append_file_str(strlog_file, strout);
    cerr << strout << endl;
    return false;
  }

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

  // ./logs/ subdirectory exists now.
  // 4) Check for the existance of a ".rotated" file
  string strRotateFile = strLogBackupDir + "." + get_short_filename(strlog_file) + ".rotated";

  // if the .rotated file does not exist, create it and return false
  if (!file_exists(strRotateFile)) {
    append_file_str(strRotateFile, ""); // Logs an error and throws an exception if there are errors.
    return false;
  }

  // The rotation takes place only if the current system date has changed since
  // the modification date of the .rotated file:
  return get_datetime_date(file_modified(strRotateFile)) != date();
#endif
}

void rotate_logfile(const string & strlog_file) {
  // A function you can call to do simple day of month-based log rotation. compressed log files
  // are stored under a "logs" sub-directory.
  #ifndef __linux__
    undefined;
    my_throw("Function only defined for linux...");
  #else
    try {
      if (!is_time_to_rotate_logfile(strlog_file)) return;

      {
        string strout = string(__FUNCTION__) + "(): rotating logfile...";
        clog << strout << endl;
        append_file_str(strlog_file, strout);
      }

      // Logfile exists?
      if (!file_exists(strlog_file)) return;

      // 1) Get the path component of the logfile path.
      string strLogDir, strLogFName;
      break_down_file_path(strlog_file, strLogDir, strLogFName);

      // 2) Build the "logs" sub-directory path
      strLogDir=ensure_last_char(strLogDir, '/');
      string strLogBackupDir = strLogDir + "logs/";

      // 3) Create the directory if it isn't found
      if (!dir_exists(strLogBackupDir)) {
        mkdir(strLogBackupDir); // Throws an exception if there is an error.
      } // end if

      // ./logs/ subdirectory exists now.

      // Create a .gz file in the logs subdirectory
      //   -- generate the filename
      datetime dtmNow = now();
      dtmNow -= 60*60*24; // Subtract a day from the current time - we want the log filename
                          // to reflect the day passed, not the new day

      // We now use the entire date in the filename, not just the month of day:
      string strbackupfile = strLogBackupDir + strLogFName + "." + format_datetime(dtmNow, "%F");

      // Move the logfile to this location:
      mv(strlog_file, strbackupfile);

      // Now compress the file:
      {
        string strout;
        system_capture_out_throw("gzip -f " + strbackupfile, strout);
      }

      // Reset the .rotated file, this lets us know when to perform the rotation again...
      {
        string strRotateFile = strLogBackupDir + "." + get_short_filename(strlog_file) + ".rotated";
        remove(strRotateFile.c_str());
        append_file_str(strRotateFile, ""); // Logs an error and throws an exception if there are any errors.
      }

      // Log a completion message.
      {
        string strmessage = "Previous logfile was archived to " + strbackupfile + ".gz";
        clog << strmessage << endl;
        append_file_str(strlog_file, strmessage);
      }

      // Remove log backup files older than 30 days here:
      {
        datetime dtmoldest = now() - 30*60*60*24;
        dir_list logs_dir(strLogBackupDir);
        while (logs_dir) {
          string strfile = logs_dir;
          if (file_modified(strLogBackupDir + strfile) < dtmoldest) {
            string strmessage = "Removing old logfile backup: " + strLogBackupDir + strfile;
            clog << strmessage << endl;
            append_file_str(strlog_file, strmessage);
            rm(strLogBackupDir + strfile);
          }
        }
      }

    }
    catch (const exception & E) {
      // If we have an exception in this function we don't throw an exception, we
      // write it to the stdout & logfile instead:
      string strout = (string) "**** " + E.what();
      cerr << strout << endl;
      append_file_str(strlog_file, strout);
    }
  #endif
}
