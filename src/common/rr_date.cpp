
#include "rr_date.h"
#include "my_time.h"
#include "exception.h"
#include "my_string.h"

using namespace std;

datetime rrdateint_to_datetime(const int intrrdate) {
  // Convert an RR date (4 digit number representing days since 1/1/1980) to a DateTime value
  if (intrrdate < 1 || intrrdate > 9998) { // Throw an error if RRDate is not in the allowed range!
    my_throw("Invalid RR date (outside allowed range 1-9998): " + itostr(intrrdate));
  }
  return make_date(1998, 1, 1) + intrrdate*60*60*24;
}

datetime rrdate_to_datetime(const string & strrrdate) {
  // Convert an RR date (4 digit number representing days since 1/1/1980) to a DateTime value
  return rrdateint_to_datetime(strtoi(strrrdate));
}

int get_rrdateint(const datetime dtmdate) {
  // Return the number of days since 01/01/1998.
  int intRet = (get_datetime_date(dtmdate) - make_date(1998, 1, 1))  / (24*60*60);
  if (intRet < 1 || intRet > 9998) {
    my_throw("Invalid RR date (outside allowed range 1-9998): " + itostr(intRet));
  }
  return intRet;
}

string datetime_to_rrdate(const datetime dtmdate) {
  // Convert a DateTime value to an RR date string
  int intDays = get_rrdateint(dtmdate); // This function throws an exception if the RR date is outside of the range 0-9998

  // Now convert to a 4-character string.
  char Buffer[10];
  sprintf(Buffer, "%04d", intDays);

  // Return the string.
  return Buffer;
}
