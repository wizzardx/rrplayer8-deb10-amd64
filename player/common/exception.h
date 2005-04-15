// Custom exception class

#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <exception>
#include <string>
#include "logging.h"

using namespace std;

// The exception:

class my_exception : public exception {
public:
  my_exception(const string & strerr, const string & strfile, const string & strfunc, const int intline);
  virtual ~my_exception() throw() {};
  virtual const char* what() const throw();

  // Return the elements of the error:
  string get_error() const;     // Error description
  string get_file() const;      // Source file name
  string get_func() const;  // Function name
  int get_line() const;         // Line number

private:
  // Error details:
  string strerr, strfile, strfunc;
  int intline;
};

// A macro to throw an exception of this type:
#define my_throw(strerr) throw my_exception(strerr, __FILE__,__FUNCTION__,__LINE__)

// A macro to catch & log exceptions:
#define catch_exceptions catch(const my_exception & E) { logging.log(LT_ERROR, E.get_error(), E.get_file(), E.get_func(), E.get_line()); } catch(const exception & E) { log_error((string)"Exception: " + E.what()); }  catch(...) { log_error ((string)"Caught an unexpected exception of type \"" + get_current_exception_type() + "\"!"); }

// Function used by the catch_exceptions macro:
string get_current_exception_type(); // Fetch the current exception type. Calls "__cxa_current_exception_type()"

#endif
