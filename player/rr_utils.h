/***************************************************************************
  rr_utils.h  -  General useful C++ functions for use in Radio Retail apps
  ------------------------------------------------------------------------

    version              : v0.26
    begin                : Mon Jun 10 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za

    contributors         : Arthur Rousseau

 ***************************************************************************/

#ifndef RR_UTILS_H
#define RR_UTILS_H
#define RR_UTILS_H_VERSION 26 // Meaning 0.26

#include <string>
#include <time.h>             // Time-related
#include <ext/hash_set>     // Hash set types

#if (!defined __linux__ && !defined __WIN32__)
  #error Either __linux__ or __WIN32__ must be defined by the system!
#endif

#ifdef __linux__
  #include <config.h>          // PACKAGE - ie app name
#elif defined __WIN32__
  // The line below will have to be changed for every project:
  #include "make_work_libraries_portable_private.h"
  #define PACKAGE PRODUCT_NAME
#endif  

#include <fstream>
#include <iostream>  // cout - Needed for testing_func, undefined_func

#include "logging.h"     // New logging classes. (v0.02)
#include "limits.h" // For INT_MIN
#include "check_library_versions.h" // Always last: Check the versions of included libraries.

using namespace std;
using namespace __gnu_cxx; // using "extended" c++. ok because our C++ > 3

// A namespace for all Radio Retail stuff: (as opposed to std, gnu_cxx, etc)
namespace rr {

  // Declare a type for representing dates and times.

  typedef time_t DateTime;  // Custom time - represented as seconds since the epoch
  const DateTime datetime_error = LONG_MIN;        // Error value returned by date/time
                                                   // functions which are unable to return
                                                   // a valid datetime. Use this constant to
                                                   // check return values for errors.

  const DateTime DATETIME_MIN=-(12*60*60); // The epoch at the -12 timezone, ie 01/01/1970 00:00, minus 6*60 seconds.
  const DateTime DATETIME_MAX=LONG_MAX;    // Probably midnight, February 5, 2036,
                                           // but recompiling close to the time should use a better 64-bit representation for dates & times?
  
  // Date/Time

  DateTime Date();

  // WARNING: date_diff does not take leap seconds into account!
  long date_diff(const string & interval, const DateTime date1, const DateTime date2);
  tm datetime_to_tm(DateTime Time);

  string format_datetime(const DateTime datetime, const string & strFormat);
  void GetDateParts(const DateTime dtmDate, int & intYear, int & intMonth, int & intDay);
  DateTime GetDateTimeDate(DateTime datetime);
  DateTime GetDateTimeTime(DateTime datetime);
  bool IsDate(const string & strDate);
  bool IsTime(const string & strTime);
  bool IsDateTime(const string & strDateTime);
  DateTime MakeDateTime(int year, int month, int day, int hour, int minute, int second); // Throws an exception if there are problems.
  inline DateTime MakeDate(int year, int month, int day) {return MakeDateTime(year, month, day, 0, 0, 0);}
  inline DateTime MakeTime(int hour, int minute, int second) {return MakeDateTime(1970, 1, 1, hour, minute, second); }
  DateTime Now();
  DateTime Time();

  inline int Year(const DateTime datetime) { int intYear, intMonth, intDay; GetDateParts(datetime, intYear, intMonth, intDay); return intYear; }
  inline int Month(const DateTime datetime) { int intYear, intMonth, intDay; GetDateParts(datetime, intYear, intMonth, intDay); return intMonth; }
  inline int Day(const DateTime datetime) { int intYear, intMonth, intDay; GetDateParts(datetime, intYear, intMonth, intDay); return intDay; }

  int Weekday(const DateTime datetime);

  DateTime ParseDateString(const string & strDate);
  DateTime ParseTimeString(const string & strTime);
  DateTime ParseDateTimeString(const string & strDateTime);

  // A macro that returns true if a year passed to it is a leap year.
  #define is_leap_year(year) __isleap(year)

  // String conversions:
  // - Boolean conversion:
  bool strtobool(const string & str);  
  bool IsBool(const string & strBool);
  string booltostr(bool blnval); // And the reverse.
  
  // - Numeric conversion:
  //   - Int:
  int strtoi(const string & str);  
  bool IsInt(const string & strNumber);
  string itostr(const int num);  
  //   - Long:
  long strtol(const string & str);  
  bool IsLong(const string & strNumber);
  string ltostr(const long num);    
  //   - Unsigned Long Long:
  unsigned long long int strtoull(const string & str);    
  bool IsUnsignedLongLong(const string & strNumber);
  //   - Double:
  double strtod(const string & str);  
  bool IsDouble(const string & strNumber);

  // Other string functions:
  string ensure_char_at_end(const string & str, const char AtEnd);
  string GetShortFileName(const string & strLongFileName);

  string Left(const string & str, const int left_chars);
  string remove_ending_cr(const string & str);
  string replace(const string & strSearchIn, const string & strSearchFor, const string & strReplaceWith, const bool CaseSensitive=true);
  string replacei(const string & strSearchIn, const string & strSearchFor, const string & strReplaceWith);
  string Right(const string & str, const int right_chars);
  
  string StringToLower(const string & str);
  string StringToUpper(const string & str);
  char * strstri(const string & SearchIn, const string & SearchFor);
  string substr(const string & str, int startpos, int len);
  string substr(const string & str, int startpos);
  string trim(const string & strStringToTrim);
  string WrapLines(const string & strText, const long lngSpaceAfter, const string & strNextLineTabChars);

  // Misc base conversion:
  // - Base 36: (0-9 and A-Z)
  unsigned long Base36ToDec(const string & strBase36);
  string DecToBase36(const unsigned long lngDecVal, const int intPlaces);  

  // Directory and file-handling
  void AppendStringToTextFile(const string & strString, const string & strFilePath);
  void break_down_file_path(const string & strfilepath, string & strfile_dir, string & strfile_name);
  void cp(const string & strsource, const string & strdest); // Logs an error and throws an exception if the copy fails.
  void df(const string & strfile, unsigned long long int & total, unsigned long long int & used, unsigned long long int & available, string & strfilesystem, string & strmounted_on);
  void ClearReadOnlyInDir(const string & strpath);
  long CountDirFiles(const string &strpath);
  long CountFileLines(const string & strfilepath);
  bool DirExists(const string & strPath);
  bool FileExists(const string & strPath);
  bool FileExists_CaseInsensitive(const string & strDir, const string & strFileName, string & strActualFileName);
  long FileSize(const string & strPath);
  bool FindTextInFile(const string & strText, const string & strFilePath);

  string getcwd();
  string GetExecPath(); // Return the directory and filename of the current process. Read from /proc/
  string GetExecDir(); // Just the directory part of GetExecPath
  DateTime GetFileDateModified(const string & strPath);

  void mkdir(const string & Path); // Throws an exception if the function fails.
  void mv(const string & strsource, const string & strdest); // Logs an error and throws an exception if the move fails.
  
  string ReadSymLink(const string & strPath);

  enum PATH_TYPE { // Used for RelPathToAbs, paths will always have '/' appended to the end...
    PATH_IS_FILE,
    PATH_IS_DIR
  };
  
  string RelPathToAbs(const string & strPath, const PATH_TYPE path_type);

  void rm(const string & strFile); // Removes a file & throws an exception if the function fails.
  void rmdir(const string & strPath); // Throws an exception if the function fails
  string string_to_unix_filename(const string & strFName);

  // Conf-file handling
  bool ifstream_find_line_starting_with(ifstream * input_file, const string & strStartText, string * strLine, const string & strErrorStartText);
  void Process_Conf_Line(const string & Line, string & Identifier, string & Value);

  // Daemon-related
  int daemon(const int nochdir, const int noclose, const string & strdirectory = "/");

  // System info
  string GetHostname();
  string GetIPAddress();
  long GetUpTime();
  
  bool pid_exists(const long lngpid);
  
  unsigned ProcessInstances(const string & strProcessName);

  // RR-date handling (RR date 1-9998 = 02/01/1998 - 17/05/2025
  // - These functions throw exceptions if an RR date is outside the range 1-9998.
  // - RR dates 0 and 9999 are excluded because they are considered suspicious...)

  // *WARNING* - These RR date functions do NOT take into account leap seconds!
  // The earth's rotation varies slightly so seconds are added to UTC time periodically.
  // see: http://www.boulder.nist.gov/timefreq/general/leaps.htm

  DateTime RRDateIntToDateTime(const int intRRDate);
  DateTime RRDateToDateTime(const string & RRDate);
  int GetRRDateInt(const DateTime Date);
  string DateTimeToRRDate(const DateTime Date); // Always returns a 4-character string.

  // Execution, capture output:
  int system_capture_out(const string & COMMAND, string & strout);

  // Communication (net send, e-mail)
  void net_send(const string & strDest, const string & strMessage);
  void send_email(const string & strFrom, const string & strTo, const string & strSubject, const string & strBody);

  // Miscelleneous system calls
  string get_current_exception_type(); // Fetch the current exception type. Calls "__cxa_current_exception_type()"
  void RestartLinux();
  void * xmalloc(size_t size); // Call malloc, check the return results, thrown an exception if an error occurs

  // String Hash Set (used for quick storing (and searching for) unqiue strings in memory)
  struct string_hash {
    size_t operator()(string s1) const {
      hash<const char *> H;
      return H(s1.c_str());
    }
  };

  struct eqstr {
    bool operator()(string s1, string s2) const {
      return s1==s2;
    }
  };

  typedef hash_set<string, string_hash, eqstr> string_hash_set;

  bool KeyInStringHashSet(string_hash_set & Set, const string & str);

  // Development macros, catching exceptions...
  #define undefined_func  cout << "UNDEFINED: " << __FUNCTION__ << "(), " << __FILE__ << ":" << __LINE__ << endl
  #define testing_func    cout << "TESTING: " << __FUNCTION__ << "(), " << __FILE__ << ":" << __LINE__ << endl

  // Print "UNDEFINED" or "TESTING" as per the above,
  // and then throw an exception:
  #define undefined_func_throw undefined_func; rr_throw("UNDEFINED")
  #define testing_func_throw   testing_func;   rr_throw("TESTING")

  #define log_unexpected_exception log_error ((string)"Caught an unexpected exception of type \"" + get_current_exception_type() + "\"!")
  //  #define catch_exceptions catch(...) { log_unexpected_exception; }
  #define catch_exceptions catch(const rr_exception & R) { Logging.do_log(R.get_error(), LT_ERROR, PACKAGE,  R.get_source_file(), R.get_source_function(), R.get_source_line()); } catch(const exception & E) { log_error((string)"Exception: " + E.what()); }  catch(...) { log_unexpected_exception; }
  #define rr_throw(strerror) throw rr_exception(strerror, __FILE__,__FUNCTION__,__LINE__)
  #define debug_print_mem_usage system(string("cat /proc/" + itostr(getpid()) + "/status | grep VmSize").c_str())

  // Class for rr_throw:
  // RR exception class: Include as much information about the location of the error as possible.
  class rr_exception : public exception {
  public:
      explicit rr_exception(const string & strerror, const string & strsource_file, const string & strsource_function, const int intsource_line) {
      this->strerror = strerror;
      this->strsource_file = strsource_file;
      this->strsource_function = strsource_function;
      this->intsource_line=intsource_line;
    }
    virtual ~rr_exception() throw() {}
    virtual const char* what() const throw() { return (strerror + " (" + strsource_file + ":" + itostr(intsource_line) + ")").c_str(); }

    // Return the elements of the error:
    string get_error() const { return strerror;  }
    string get_source_file() const { return strsource_file; }
    string get_source_function() const { return strsource_function; }
    int get_source_line() const { return intsource_line; }

  private:
    string strerror;
    string strsource_file;
    string strsource_function;
    int intsource_line;
  };
  
}

#endif

