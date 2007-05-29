/// @file
/// RR date handling

#include "my_time.h"

datetime rrdateint_to_datetime(const int intrrdate);
datetime rrdate_to_datetime(const string & strrrdate);
int get_rrdateint(const datetime dtmdate);
string datetime_to_rrdate(const datetime dtmdate); /// Always returns a 4-character string.
