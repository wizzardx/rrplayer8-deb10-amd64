
#ifndef TESTING_H
#define TESTING_H

#include <iostream>
#include "exception.h"
#include "file.h"

// Development macros, catching exceptions...
#define undefined  cout << "UNDEFINED: " << __FUNCTION__ << "(), " << get_short_filename(__FILE__) << ":" << __LINE__ << endl
#define testing    cout << "TESTING: " << __FUNCTION__ << "(), " << get_short_filename(__FILE__) << ":" << __LINE__ << endl

// Print "UNDEFINED" or "TESTING" as per the above,
// and then throw an exception:
#define undefined_throw undefined; my_throw("UNDEFINED")
#define testing_throw   testing;   my_throw("TESTING")

#endif
