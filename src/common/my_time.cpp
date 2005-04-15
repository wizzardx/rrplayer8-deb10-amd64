#include "my_time.h"
#include "my_string.h"
#include "exception.h"
#include "testing.h"

// Fetch current date & time:

// Current date & time
datetime now() {
  // Return the current date/time
  time_t curr;
  time(&curr);
  return (curr);
}

// Current date, no time (ie, time is 12:00:00 AM)
datetime date() {
  // Return the current Date (with no Time info)
  return get_datetime_date(now());
}

// Current time, no date (ie, date is 1970-01-01)
datetime time() {
  // Return the current Time (with no Date info)
  return get_datetime_time(now());
}

// Building a datetime variable:

datetime make_datetime(int year, int month, int day, int hour, int minute, int second) {
  // Check the date - libc's mktime won't check for errors...

  // Check the year:
  if (year < 1970) {
    my_throw("Bad year " + itostr(year) + " - year must be 1970 or later");
  }

  // Check the month:
  if ((month < 1) || (month > 12)) {
    my_throw("Bad month " + itostr(month) + " - month must be between 1 and 12");
  }

  // Check the day:
  int month_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

  // Is the year a leap year?
  if (is_leap_year(year)) month_days[1] = 29;
  if ((day < 0) || (day > month_days[month - 1])) {
    my_throw("Bad day of month " + itostr(day) + " - day must be between 1 and " + itostr(month_days[month - 1]));
  }

  // Check the hour:
  if ((hour) < 0 || (hour > 23)) {
    my_throw("Bad hour " + itostr(hour) + ". Hour must be between 0 and 23.");
  }

  // Check the minute:
  if ((minute < 0) || (minute > 59)) {
    my_throw("Bad minute " + itostr(minute) + ". Minute must be between 0 and 59" );
  }

  // Check the second:
  // Check the minute:
  if ((second < 0) || (second > 59)) {
    my_throw("Bad second " + itostr(second) + ". Second must be between 0 and 59" );
  }

  // The function's args are fine, so now build the tm_time structure and call mktime

  // Build a DateTime (time_t) variable from the components of a date
  struct tm tm_time; // Declare
  memset(&tm_time, 0, sizeof(tm)); // Initialize with 0s

  // Fill in the fields
  tm_time.tm_year = year - 1900;
  tm_time.tm_mon  = month - 1;
  tm_time.tm_mday = day;

  tm_time.tm_hour = hour;
  tm_time.tm_min  = minute;
  tm_time.tm_sec  = second;

  // Convert to a time_t value and return
  datetime dtmret = mktime(&tm_time);

  // -1 is returned if mktime failed, or if the date&time to be converted
  // is exactly 1970-01-01 01:59:59 (with the South Africa timezone of +2)
  if (dtmret == -1) {
    // Check: Is the date & time this exact time which would cause a non-error -1
    // to be returned?
    tm tm_check = datetime_to_tm(-1);
    if (tm_time.tm_year   != tm_check.tm_year ||
        tm_time.tm_mon    != tm_check.tm_mon  ||
        tm_time.tm_mday   != tm_check.tm_mday ||
        tm_time.tm_hour   != tm_check.tm_hour ||
        tm_time.tm_min    != tm_check.tm_min  ||
        tm_time.tm_sec    != tm_check.tm_sec
        
        #ifdef __linux__
          // tm_gmtoff is not currently available under mingw!
          || tm_time.tm_gmtoff != tm_check.tm_gmtoff
        #endif
        
        ) {
        // Nope, at least one of the fields is different.
        // mktime really did have an error.
      my_throw("logic error - mktime call failed.");
    }
  }
  return dtmret;
}

// Extracting portions of a datetime variable:

void get_date_parts(const datetime dtmdate, int & intyear, int & intmonth, int & intday) {
  // Convert DateTime dtmdate to tm format & extract parts
  tm time  = datetime_to_tm(dtmdate);
  intyear  = 1900 + time.tm_year;
  intmonth = time.tm_mon + 1;
  intday   = time.tm_mday;
}

void get_time_parts(const datetime dtmtime, int & inthour, int & intminute, int & intsecond) {
  // Convert DateTime dtmtime to tm format & extract parts
  tm time   = datetime_to_tm(dtmtime);
  inthour   = time.tm_hour;
  intminute = time.tm_min;
  intsecond = time.tm_sec;
}

datetime get_datetime_date(datetime dt) {
  // Return the DateTime value, but without the time part
  // Doing it this way instead of doing mahts. Daylight savings stuff really messes around with the maths!
  tm tm_datetime = datetime_to_tm(dt);

  // Reset the time portion to the epoch:
  tm_datetime.tm_hour = 0;
  tm_datetime.tm_min  = 0;
  tm_datetime.tm_sec  = 0;

  // Convert back to a time_t value, and return:
  return mktime(&tm_datetime);
}

datetime get_datetime_time(datetime dt) {
  // Convert into a broken-down format:
  // Doing it this way instead of doing maths. Daylight savings stuff really messes around with the maths!
  tm tm_datetime = datetime_to_tm(dt);

  // Reset the date portion to the epoch:
  tm_datetime.tm_year = 70;
  tm_datetime.tm_mon  = 0;
  tm_datetime.tm_mday = 1;

  // Convert back to a time_t value, and return:
  return mktime(&tm_datetime);
}

int weekday(const datetime dt) {
  // Based on the VB Weekday() function:
  // Assume Monday = day1, Sunday = day7
  tm tm_datetime = datetime_to_tm(dt);

  int day = tm_datetime.tm_wday;
  if (day==0) day = 7; // Convert Sun-Sat(0-6) to Mon-Sun(1-7)
  return day;
}


// Convert time_t to broken-down format
tm datetime_to_tm(datetime dt) {
  // Convert time_t representation (seconds) to tm (structured record) representation   
  #ifdef __linux__
    // Linux has the (thread-safe) localtime_r function:
    tm ret;
    tm *pret = localtime_r(&dt, &ret);
    if (pret != &ret) {
      my_throw("localtime_r() failed. datetime value: " + ltostr(dt));
    }    
    // Return the result:
    return(ret);
  #else
    // Windows doesn't, so use localtime instead:
    tm *pret = localtime(&dt);
    if (pret == NULL) {
      my_throw("localtime() failed. datetime value: " + ltostr(dt));       
    }
    return(*pret);
  #endif
}

// STRING-HANDLING

// - String represents a valid date/time?
bool isdate(const string & str) {
  // Check whether strDate is a valid representation of a date
  try {
    parse_date_string(str);
    // No exception, strDate is a valid date string.
    return true;
  }
  catch(...) {
    // ParseDateString threw an exception, not a valid date string!
    return false;
  }
}

bool istime(const string & str) {
  // Check whether strTime is a valid representation of a time
  try {
    parse_time_string(str);
    return true; // ParseTimeString did not throw an exception, this is a valid time string
  }
  catch(...) {
    return false; // ParseTimeString threw an exception, this is an invalid time string.
  }
}

bool isdatetime(const string & str) {
  // Check whether strTime is a valid representation of a time
  try {
    parse_datetime_string(str);      
    return true; // ParseDateTimeString did not throw an exception, this is a valid date/time string.
  }
  catch(...) {
    // ParseDateTimeString threw an exception, this is an invalid date/time string!
    return false;
  }
}

// - Extract datetime from a string:

datetime parse_date_string(const string & strdate) {
  // Take a simple string of the layout dd/mm/yyyy and convert it to a DateTime var
  const char * DateStr = strdate.c_str();
  if (!(isdigit(DateStr[0]) && isdigit(DateStr[1]) && DateStr[2] == '/' &&
      isdigit(DateStr[3]) && isdigit(DateStr[4]) && DateStr[5] == '/' &&
      isdigit(DateStr[6]) && isdigit(DateStr[7]) && isdigit(DateStr[8]) && isdigit(DateStr[9]))) {
    my_throw("Bad date string");      
  }

  int intDay = strtoi(substr(strdate, 0, 2));
  int intMonth = strtoi(substr(strdate, 3, 2));
  int intYear = strtoi(substr(strdate, 6, 4));

  datetime dtmDate = make_date(intYear, intMonth, intDay);
  string strTestDate = format_datetime(dtmDate, "%d/%m/%Y");
  if (strTestDate != strdate) {
    my_throw("Date string failed a test!");
  }

  // Now return the parsed date:
  return dtmDate;      
}

datetime parse_time_string(const string & strtime) {
  // Take a simple string of the layout "xx:xx:xx am/pm" and convert it to a DateTime var.
  // - All fields after the first 2 xx groups are optional.
  datetime dtmReturn = datetime_error;

  string strwork = trim(lcase(strtime));
  if (strwork.length() < 5)  {
    my_throw("Time string is too short!");
  }
  
  // We have enough characters for a "hh:nn" time.
  const char * c = strwork.c_str();
  
  // Check the format of these 5 chars.
  if (!(isdigit(c[0]) && isdigit(c[1]) && (c[2] == ':') && isdigit(c[3]) && isdigit(c[4]))) {
    my_throw("Bad time format in the first 5 chars!");
  }

  // The first 5 chars are good. Extract the values.
  int intNum1 = strtoi(strwork.substr(0, 2));
  int intNum2 = strtoi(strwork.substr(3, 2));
  int intNum3 = 0; // Default value if not found.
  string strAMPM = ""; // At the end, this will be "", "am", or "pm"

  // Now check for more characters after the minutes part:

  unsigned int pos = 4; // The position of the last extracted character
  if (strwork.length() > (pos + 1)) {
    // There are more characters after the minutes part
    // Check for a ':', indicating there is a 3rd numeric element.
    if (c[pos + 1] != ':') {
      my_throw("6th character is not ':'");
    }

    // The 6th character is a ':' check for 2 more digit characters
    if (strwork.length() < pos + 4) {
      // There was a 2nd ':' character, but not enough characters after.
      my_throw("Not enough characters after the 2nd ':'");
    }
    
    // 2 more characters found. Their type?
    if (!(isdigit(c[pos + 2]) && isdigit(c[pos + 3])))  {

      my_throw("Invalid 'seconds' characters found!");
    }
      
    // Numeric, so extract them:
    intNum3 = strtoi(strwork.substr(pos + 2, 2));

    pos = 7; // The position of the last extracted character
    
    // Now work out if there is a remaining "am" or "pm" section (converted to lowercase earlier)
    if (strwork.length() > (pos + 1)) {
      // There are more characters.
      // Are there exactly the right remaining number of characters?
      if (strwork.length() == (pos + 4)) {
        // Is there 1 space and then an 'a' or 'p', and then an 'm' ?
        if ((c[pos+1] == ' ') && ((c[pos+2] == 'a') || (c[pos+2] == 'p')) && (c[pos+3] == 'm')) {

          strAMPM = substr(strwork, pos+2, 2);
        }
        else {
          // Incorrect characters found!
          my_throw("Invalid characters found in the am/pm part of the time string");
        }
      }
      else {
        // There are additional characters, but not the correct amount!
        my_throw("Bad number of characters found in the am/pm part of the time string");
      }
    } // otherwise, there is no AM/PM section.
  } // END: if (strwork.length() > (pos + 1)) // Check for more characters after the minute.

  // Check the range of the extracted numbers...
  // if an "am" or "pm" value was specified, then the hour cannot be more than 12...
  int intMaxHour = (strAMPM == "") ? 23 : 12;
  if (!((intNum1 >= 0) && (intNum1<= intMaxHour) && // Hour
      (intNum2 >= 0) && (intNum2 <= 59) && // Minutes
      (intNum3 >= 0) && (intNum3 <= 59))) {
    my_throw("Hours, minutes, or seconds are out of range.");
  }

  // Now piece together the extracted parts, and make the date to return.
  // - Generate the base time
  dtmReturn = make_time(intNum1, intNum2, intNum3);
  // 12 is a special-case hour:
  if (intNum1 == 12) {
    // Is there an "AM" on the end, then subtract 12 hours:
    if (strAMPM == "am") {
      dtmReturn -= (12*60*60); // DateTimes are represented as seconds since the epoch...

    }
    else if (strAMPM == "pm") {
      // The hour is a PM, and it is not 12 PM. Therefore we add 12 hours.
      dtmReturn += (12*60*60); // DateTimes are represented as seconds since the epoch...
    }
  } // end if (check numeric ranges)

  // Now return the results
  return dtmReturn;
}

datetime parse_datetime_string(const string & strdatetime) {
  // Take string var, see if it has date or time portions, and attempt to extract the portions.
  // The acceptible layouts are: "[date]", "[date] [time]" and "[time] [date]" where
  // [date] is in the format "dd/mm/yyyy" and time is in the format "hh:nn:ss ?m". However,
  // with [time], the ":ss" and "?m" parts are optional (ps: ?m means AM or PM)

  datetime dtmRet = datetime_error; // Reset the return result...

  // Pull the date/time sections out of the string...
  string strTime = "";
  string strDate = "";

  // Is there a date portion? Search for the first "/" character
  string strwork = lcase(trim(strdatetime));
  int intPos = strwork.find("/");

  if (intPos < 0) {
    // There were no "/" characters found. Ie the string probably is just a time
    strTime = strwork;
  }
  else {

    // A "/" character was found. ie there is probably a date in the string. Check where it was found.
    if (intPos == 2) {
      // The 3rd character was a "/". This means that the date would be in the beginning.
      strDate = substr(strwork, 0, 10);
      // If there is any time, it is after this point.
      strTime = trim(substr(strwork, 10)); // Remove a string from the start if there is one.
    }
    else {
      // "/" was found at a different character position. This means that the date is probably
      // found after the time
      // - Extract the section before the date
      strTime = substr(strwork, 0, intPos - 3);
      // - Now extract the date.
      strDate = trim(substr(strwork, intPos - 3));
    }
  }

  // Was anything extracted?
  if (strDate == "" && strTime == "") {
    // No:
    my_throw("Could not extract a date or time!");
  }

  // We now have any date and time portions extracted from their various positions within
  // the string. Now attempt to convert to a datetime.
  datetime dtmDate = datetime_error;
  datetime dtmTime = datetime_error;

  // Was a date string extracted?
  if (strDate != "") {
    dtmDate = parse_date_string(strDate); // This will throw an exception if parsing fails...
    dtmRet = dtmDate;
  }

  // Was a time string extracted?
  if (strTime != "") {
    dtmTime = parse_time_string(strTime); // This will throw an exception if parsing fails
    dtmRet = dtmTime;
  }

  // Did we extract both a date and time string?

  if ((strDate != "") && (strTime != "")) {
    // Add the 2 parsed times together:
    testing_throw; // Have to do this part differently! See get_datetime_time;
    dtmRet = dtmDate + dtmTime - timezone; // Add the time and subtract the timezone variable (ie: datetime maths)    
  }

  return dtmRet;
}

// - Output datetime variable as a string
string format_datetime(const datetime dt, const string & strformat) {
  char buffer[256];
  buffer[0]='\1'; // For error checking

  /* Convert the datetime var to local time representation. */
  tm loctime = datetime_to_tm(dt);
     
  /* Print it out in a nice format. */
  size_t len = strftime (buffer, 256, strformat.c_str(), &loctime);
  
  if (len == 0 && buffer[0] != '\0') {
    my_throw("There was an error in the call to strftime()!");
  }
  
  // No errors, return the formatted string
  return buffer;
}

// MISCELLENEOUS

// WARNING: date_diff does not take leap seconds into account!
long date_diff(const string & interval, const datetime date1, const datetime date2) {
  // This is an adaption of Visual Basic's "DateDiff" function...
  // WARNING: This function does not take leap seconds into account!
  // see: http://www.boulder.nist.gov/timefreq/general/leaps.htm

  // Check which interval is being requested, and return the requested
  // interval:

  if (interval == "yyyy") {
    // - How many years from date1 to date2?
    int intyear1; // First year
    int intyear2; // Second year
    int intdummy; // Throw-away arg.
    get_date_parts(date1, intyear1, intdummy, intdummy);
    get_date_parts(date2, intyear2, intdummy, intdummy);
    return intyear2-intyear1;
  }
  else if (interval == "q") {
    // Quarter
    int intyear1; // First year
    int intyear2; // Second year
    int intmonth1; // First month
    int intmonth2; // Second month
    int intdummy; // Thrown away argument.
    get_date_parts(date1, intyear1, intmonth1, intdummy);
    get_date_parts(date2, intyear2, intmonth2, intdummy);

    // Convert the dates into quarters:
    int intquarter1=intyear1*4+(intmonth1+2)/3;

    int intquarter2=intyear2*4+(intmonth2+2)/3;

    // Return the difference:
    return intquarter2-intquarter1;      
  }
  else if (interval == "m") {
    // Month
    int intyear1; // First year
    int intyear2; // Second year
    int intmonth1; // First month
    int intmonth2; // Second month
    int intdummy; // Scrapped argument.
    get_date_parts(date1, intyear1, intmonth1, intdummy);
    get_date_parts(date2, intyear2, intmonth2, intdummy);

    // Return the difference in months between the 2 dates:
    return (intyear2*12+intmonth2) - (intyear1*12+intmonth1);      
  }
  else if (interval == "y" || interval == "d") {
    // Day of year OR Day (same thing)
    testing_throw; // Have to do this part differently! See get_datetime_time;    
    return (date2-timezone)/(24*60*60) - (date1-timezone)/(24*60*60);
  }
  else if (interval == "w") {
    // Weekday
    // How many weeks between date1 and date2? ie, if date1 is on a Monday then count Mondays,
    // not including date1...

    // Remove the time parts:
    // - eg: 11/08/2004 17:15:18 becomes 11/08/2004 00:00
    datetime dtmfrom = get_datetime_date(date1);
    datetime dtmto   = get_datetime_date(date2);

    // Now calculate how many 7-day weeks are between these:
    return (dtmto-dtmfrom)/(7*24*60*60);                        
  }
  else if (interval == "ww") {
    // Week
    // - How many *CALENDAR* weeks between date1 and date2? Calendar weeks start on Monday.

    // Fetch local copies of the args for manipulation...
    datetime time1 = date1;
    datetime time2 = date2;
    
    // Remove the time parts:
    // - eg: 11/08/2004 17:15:18 becomes 11/08/2004 00:00
                
    time1 = get_datetime_date(date1);
    time2 = get_datetime_date(date2);
    
    // Now reset these values to the beginning of their respective weeks
    // eg: 11/08/2004 (Wed) becomes 09/08/2004 (Mon)
    
    time1 = get_datetime_date(time1) - ((24*60*60) * (weekday(time1)-1));
    time2 = get_datetime_date(time2) - ((24*60*60) * (weekday(time2)-1));
    
    // - Now both dtmfrom and dtmto are set to Monday on the start of their respective weeks.
    // - Now return how many 7-day weeks are between the 2:      
    return (time2-time1)/(7*24*60*60);
  }
  else if (interval == "h") {
    // Hour
    testing_throw; // Have to do this part differently! See get_datetime_time;    
    return  (date2-timezone)/(60*60) - (date1-timezone)/(60*60);
  }
  else if (interval == "n") {
    // Minute

    testing_throw; // Have to do this part differently! See get_datetime_time;    
    return (date2-timezone)/60 - (date1-timezone)/60;      
  }
  else if (interval == "s") {
    // Second
    return (date2-date1);
  }
  else {
    // Bad interval
    my_throw("Unknown interval string: \"" + interval + "\"");
  }

  // If execution reaches here we have a problem!
  my_throw("Logic error!");
}

int get_num_month_days(const int intyear, const int intmonth) {
  int month_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  
  // Check arguments
  if (intyear<1970 || intyear>9999) my_throw("Year " + itostr(intyear) + " is out of range!");
  if (intmonth<1 || intmonth>12) my_throw("Month " + itostr(intmonth) + " is  out of range!");

  // Fetch # days in the month
  int intret=month_days[intmonth - 1];
  
  // If year is a leapyear and month is feb, then return 29 days instead:
  if (intmonth==2 && is_leap_year(intyear)) intret = 29;
  
  return intret;
}

// Normalize a timeval record (ie, check if the tv_usec field is too high/low, and adjust the record)
void normalise_timeval(timeval & tv) {
  // Check the range of tv.tv_usec:
  if (tv.tv_usec > 1000000) {
    // usec is too high! Adjust sec and usec.
    tv.tv_sec += (tv.tv_usec / 1000000);
    tv.tv_usec = (tv.tv_usec % 1000000);    
  }
  else if (tv.tv_usec < 0) {
    // usec is negative! Adjust sec and usec
    tv.tv_sec  = tv.tv_sec - 1 + tv.tv_usec / 1000000; // Result of / 100000 is negative...
    tv.tv_usec = 1000000 + tv.tv_usec % 1000000; // Result of % 100000 is negative...
  }
}

// Instructions automatically run at program start:
static class time_autostart {
public:
  time_autostart() {
    // Init the glibc-level "timezone" variable...
    tzset();
  }
} time_autostart;
