
#ifndef TESTING_H
#define TESTING_H

#include <iostream>
#include "exception.h"

// Development macros, catching exceptions...
#define undefined  cout << "UNDEFINED: " << __FUNCTION__ << "(), " << __FILE__ << ":" << __LINE__ << endl
#define testing    cout << "TESTING: " << __FUNCTION__ << "(), " << __FILE__ << ":" << __LINE__ << endl

// Print "UNDEFINED" or "TESTING" as per the above,
// and then throw an exception:
#define undefined_throw undefined; my_throw("UNDEFINED")
#define testing_throw   testing;   my_throw("TESTING")

#endif
