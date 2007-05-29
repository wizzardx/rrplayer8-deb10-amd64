/// @file
/// A custm logging system infrastructure.
/// Call log_line, log_message and log_error in your code. You can define
/// a custom callback function which performs application-specific logging
/// (eg: database).

#ifndef LOGGING_H
#define LOGGING_H

#include <string>
#include <ext/slist>

// Use these macros for logging:
// eg: log_message("Hello");

#define log_line(strdesc)    logging.log(LT_LINE,    strdesc, __FILE__, __FUNCTION__, __LINE__)
#define log_message(strdesc) logging.log(LT_MESSAGE, strdesc, __FILE__, __FUNCTION__, __LINE__)
#define log_warning(strdesc) logging.log(LT_WARNING, strdesc, __FILE__, __FUNCTION__, __LINE__)
#define log_error(strdesc)   logging.log(LT_ERROR,   strdesc, __FILE__, __FUNCTION__, __LINE__)

using namespace std;
using namespace __gnu_cxx;

/// Error types
enum log_type {
  LT_LINE,    ///< Formatting output (eg: dots & lines, etc). Logging functions shouldn't do database output.
  LT_MESSAGE, ///< A general status output message. Can be logged to database
  LT_WARNING, ///< A warning message. eg, the software detects a problem, but it can continue anyway.
  LT_ERROR    ///< A serious problem which a user should attend to.
};

// Structure passed to callback loggers
class log_info {
public:
  log_type LT;    ///< What type of log event is this?
  string strdesc; ///< A string description of the log
  string strfile; ///< What source code file did the log come from?
  string strfunc; ///< What source code function did the log come from?
  int intline;    ///< What line in the source code file did the log come from?
};

/// Use this class for manipulating your applications logging
class clogging {
public:
  /// Client apps pass "callback" functions to this class.
  void add_logger(void(*func)(const log_info&));

  /// Don't call this yourself, reather use log_message, log_warning, etc.
  void log(const log_type LT, const string & strdesc, const string & strfile, const string & strfunc, const int & intline);

private:
  typedef void (callback_func)(const log_info&);

  typedef slist <callback_func *> callback_slist;
  /// A (singly-linked) list of the callbacks:
  callback_slist callbacks;
};

extern clogging logging;

// Formatting log_info records into human-readable strings:

/// The default log format. Passed to the format_log() function
const string strstandard_log_format = "[%DATE %TIME] %TYPE_NOT_LOG%MESSAGE%SOURCE_ERROR";

/// Display the contents of a log_info record in a string, in the specified format
string format_log(const log_info & log_info, const string strformat);

/// A function you can call to do simple day of month-based log rotation. compressed log files
/// are stored under a "logs" sub-directory.
void rotate_logfile(const string & strlog_file);

#endif
