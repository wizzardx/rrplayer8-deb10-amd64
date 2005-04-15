
#ifndef MY_TIME_H
#define MY_TIME_H

#include <time.h>
#include <limits.h>
#include <string>

using namespace std;

// Declare a type for representing dates and times.

typedef time_t datetime;  // Custom time - represented as seconds since the epoch

// Some useful datetime-related constants:

const datetime datetime_error = LONG_MIN;  // Meaning invalid date or time.
                                           // You can use this as the default value 
                                           // of datetime variables before they get set.
                                                 
const datetime DATETIME_MIN=-(12*60*60); // The epoch at the -12 timezone, ie 01/01/1970 00:00, minus 6*60 seconds.
const datetime DATETIME_MAX=LONG_MAX;    // Probably midnight, February 5, 2036,
                                         // but recompiling close to the time should use a better 64-bit representation for dates & times?
                                         
// Fetch current date & time:

datetime now();   // Current date & time
datetime date();  // Current date, no time (ie, time is 12:00:00 AM)
datetime time();  // Current time, no date (ie, date is 1970-01-01)

// Building a datetime variable:

datetime make_datetime(int year, int month, int day, int hour, int minute, int second);   
inline datetime make_date(int year, int month, int day) {return make_datetime(year, month, day, 0, 0, 0);}
inline datetime make_time(int hour, int minute, int second) {return make_datetime(1970, 1, 1, hour, minute, second);}

// Extracting portions of a datetime variable:

void get_date_parts(const datetime dtmdate, int & intyear, int & intmonth, int & intday);
void get_time_parts(const datetime dtmtime, int & inthour, int & intminute, int & intsecond);

datetime get_datetime_date(datetime dt);
datetime get_datetime_time(datetime dt);

inline int year(const datetime dt) {int intyear, intmonth, intday; get_date_parts(dt, intyear, intmonth, intday); return intyear;}
inline int month(const datetime dt) {int intyear, intmonth, intday; get_date_parts(dt, intyear, intmonth, intday); return intmonth;}
inline int day(const datetime dt) {int intyear, intmonth, intday; get_date_parts(dt, intyear, intmonth, intday); return intday;}
inline int hour(const datetime dt) {int inthour, intminute, intsecond; get_time_parts(dt, inthour, intminute, intsecond); return inthour;}
inline int minute(const datetime dt) {int inthour, intminute, intsecond; get_time_parts(dt, inthour, intminute, intsecond); return intminute;}
inline int second(const datetime dt) {int inthour, intminute, intsecond; get_time_parts(dt, inthour, intminute, intsecond); return intsecond;}

int weekday(const datetime dt);

tm datetime_to_tm(datetime dt); // Convert time_t to broken-down format

// STRING-HANDLING

// - String represents a valid date/time?
bool isdate(const string & str);
bool istime(const string & str);
bool isdatetime(const string & strDateTime);

// - Extract datetime from a string:

datetime parse_date_string(const string & strdate);
datetime parse_time_string(const string & strtime);
datetime parse_datetime_string(const string & strdatetime);

// - Output datetime variable as a string
string format_datetime(const datetime dt, const string & strformat);

// MISCELLENEOUS

// WARNING: date_diff does not take leap seconds into account!
long date_diff(const string & interval, const datetime date1, const datetime date2);

// Count days in a month:
int get_num_month_days(const int intyear, const int intmonth);

// Normalise a timeval record (ie, check if the tv_usec field is too high/low, and adjust the record)
void normalise_timeval(timeval & tv);

// is_leap_year: A macro that returns true if a year passed to it is a leap year.
#ifndef __isleap // Should be defined, but in case it isn't
/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
# define __isleap(year)	\
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif

#define is_leap_year(year) __isleap(year)

#endif
