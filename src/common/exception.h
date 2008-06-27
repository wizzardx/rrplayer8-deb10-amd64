/// @file
/// exception-related logic.
/// Useful for providing detailed debugging information in logs.

#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <cstring>
#include <errno.h>
#include <exception>
#include <string>

#include "logging.h"
#include "my_string.h"

using namespace std;

/// An exception class, thrown by my_throw
class my_exception : public exception {
public:
  my_exception(const string & strerr, const string & strfile, const string & strfunc, const int intline);
  virtual ~my_exception() throw() {};
  virtual const char* what() const throw();

  // Return the elements of the error:
  string get_error() const; ///< Error description
  string get_file() const;  ///< Source file name
  string get_func() const;  ///< Function name
  int get_line() const;     ///< Line number

private:
  // Error details:
  string strerr, strfile, strfunc;
  int intline;
};

/// A macro to throw an exception of this type
#define my_throw(strerr) throw my_exception(strerr, __FILE__, __FUNCTION__, __LINE__)

/// A macro to log exceptions
#define log_exception(E) logging.log(LT_ERROR, E.get_error(), E.get_file(), E.get_func(), E.get_line())

/// A macro to catch & log exceptions
#define catch_exceptions catch(const my_exception & E) { log_exception(E); } catch(const exception & E) { log_error((string)"Exception: " + E.what()); }  catch(...) { log_error ((string)"Caught an unexpected exception of type \"" + get_current_exception_type() + "\"!"); }

/// Run a libc function, and throw an exception if the result is negative.
/// If no error occured then return the result of the libc function call
#define CHECK_LIBC(expr, strdescr) check_libc(expr, strdescr, __FILE__, __FUNCTION__, __LINE__)

inline int check_libc(const int intret, const string & strdescr, const string & strfile, const string & strfunc, const int intline) {
  if (intret < 0) throw my_exception((string)strerror(errno) + ". " + strdescr, strfile, strfunc, intline);
  return intret;
}

/// Conveniance macro to thrown an exception based on the current errno value.
/// Useful for cases where CHECK_LIBC is not usable.
/// (eg: fopen returns NULL on error, not a negative value)
#define libc_throw(strdescr) my_throw((string)strerror(errno) + ". " + strdescr);

/// Function used by the catch_exceptions macro
string get_current_exception_type(); // Fetch the current exception type. Calls "__cxa_current_exception_type()"

/// Convert my_exception classes to log_info records
log_info my_exception_to_log_info(const my_exception & e);

/// Use this macro when checking for situations that shouldn't be possible (ie, bugs)
#define LOGIC_ERROR my_throw("Internal Logic Error! Please check the source code ASAP!")

#endif
