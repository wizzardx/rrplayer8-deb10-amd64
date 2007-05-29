
#include "exception.h"
#include "my_string.h"
#include "cxxabi.h"
#include "file.h"
#include <iostream>

using namespace std;
using namespace __cxxabiv1;

// Constructor:
my_exception::my_exception(const string & strerr, const string & strfile, const string & strfunc, const int intline) {
  this->strerr  = strerr;
  this->strfile = get_short_filename(strfile);
  this->strfunc = strfunc;
  this->intline = intline;
}

// Return a description of the exeption:
const char* my_exception::what() const throw() {
  // Static storage for the result, ie in main memory, not on the stack:
  static string strret = "";
  strret = strerr + " (" + strfile + ":" + itostr(intline) + ")";
  return strret.c_str();
}

// Return the elements of the error:

// Error description
string my_exception::get_error() const {
  return strerr;
}

// Source file name
string my_exception::get_file() const {
  return strfile;
}

// Function name
string my_exception::get_func() const {
  return strfunc;
}

// Line number
int my_exception::get_line() const {
  return intline;
}

/// Instructions automatically run at program start
static class exceptions_autostart {
public:
  exceptions_autostart() {
    // If an exception terminates the program, use the gnu c++ verbose error handler:
    std::set_terminate (__gnu_cxx::__verbose_terminate_handler);
  }
} exceptions_autostart;

// - Function used by the catch_exceptions macro:
string get_current_exception_type() {
  // Fetch the type of the current exception. eg "int", "double", "my_exception".
  // Based on this code: http://gcc.gnu.org/onlinedocs/libstdc++/libstdc++-html-USERS-3.3/vterminate_8cc-source.html
  string strret = "";
  type_info *t = __cxa_current_exception_type();
  if (t) {
    char const *name = t->name();
    // Note that "name" is the mangled name.
    {
       int status = -1;
       char *dem = 0;

       // Disabled until __cxa_demangle gets the runtime GPL exception.
       dem = __cxa_demangle(name, 0, 0, &status);

       strret = status == 0 ? dem : name;

       if (status == 0)
         free(dem);
     }
  }
  return strret;
}

// Convert a my_exception class to a log_info record.
log_info my_exception_to_log_info(const my_exception & e) {
  log_info LI;
  LI.LT      = LT_ERROR;
  LI.strdesc = e.get_error();
  LI.strfile = e.get_file();
  LI.strfunc = e.get_func();
  LI.intline = e.get_line();
  return LI;
}
