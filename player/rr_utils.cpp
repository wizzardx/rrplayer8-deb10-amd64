/***************************************************************************
  rr_utils.cpp  -  General useful C++ functions for use in Radio Retail apps
  --------------------------------------------------------------------------

    version              : v0.26
    begin                : Mon Jun 10 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za

    contributors         : Arthur Rousseau

 ***************************************************************************/
 
#include "rr_utils.h"

#include <stdio.h>       // I/O
#include <unistd.h>      // Linux system calls and types
#include <sys/stat.h>    // Directory info
#include <fcntl.h>       // Daemon include

#ifdef __linux__ // sys/sysinfo.h is only available for linux...
  #include <sys/sysinfo.h> // System info
#endif

#include <fstream>     // File-handling

#ifdef __linux__ // pwd.h is only available for linux...
  #include <pwd.h> // getpwuid (Fetch a users home directory)
#endif

#include "dir_list.h"
#include "temp_dir.h" // Needed for send_email()
#include <string.h> // strerr() - the string description of a stdlib error code.
#include "string_splitter.h"

// For get_current_exception_type():
#include <cxxabi.h>
using namespace abi;

namespace rr {
   /***************************************************************************
   *   Stuff automatically run at program startup...
   ***************************************************************************/
   static class rr_utils_auto_startup {
   public:
    rr_utils_auto_startup() { // Constructor will be automatically run...
      // Random number generator
      srand(time(0));
      // Init the glibc-level "timezone" variable...
      tzset();
      // If an exception terminates the program, use the gnu c++ verbose error handler:
      std::set_terminate (__gnu_cxx::__verbose_terminate_handler);
    }
   } rr_utils_startup;

   /***************************************************************************
   *   Date/Time                                                             *
   ***************************************************************************/

  DateTime Date() {
    // Return the current Date (with no Time info)
    return GetDateTimeDate(Now());
  }

  long date_diff(const string & interval, const DateTime date1, const DateTime date2) {
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
      GetDateParts(date1, intyear1, intdummy, intdummy);
      GetDateParts(date2, intyear2, intdummy, intdummy);
      return intyear2-intyear1;
    }
    else if (interval == "q") {
      // Quarter
      int intyear1; // First year
      int intyear2; // Second year
      int intmonth1; // First month
      int intmonth2; // Second month
      int intdummy; // Thrown away argument.
      GetDateParts(date1, intyear1, intmonth1, intdummy);
      GetDateParts(date2, intyear2, intmonth2, intdummy);

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
      GetDateParts(date1, intyear1, intmonth1, intdummy);
      GetDateParts(date2, intyear2, intmonth2, intdummy);

      // Return the difference in months between the 2 dates:
      return (intyear2*12+intmonth2) - (intyear1*12+intmonth1);      
    }
    else if (interval == "y" || interval == "d") {
      // Day of year OR Day (same thing)
      return (date2-timezone)/(24*60*60) - (date1-timezone)/(24*60*60);
    }
    else if (interval == "w") {
      // Weekday
      // How many weeks between date1 and date2? ie, if date1 is on a Monday then count Mondays,
      // not including date1...

      // Remove the time parts:
      // - eg: 11/08/2004 17:15:18 becomes 11/08/2004 00:00
      DateTime dtmfrom = GetDateTimeDate(date1);
      DateTime dtmto   = GetDateTimeDate(date2);

      // Now calculate how many 7-day weeks are between these:
      return (dtmto-dtmfrom)/(7*24*60*60);                        
    }
    else if (interval == "ww") {
      // Week
      // - How many *CALENDAR* weeks between date1 and date2? Calendar weeks start on Monday.

      // Fetch local copies of the args for manipulation...
      DateTime time1 = date1;
      DateTime time2 = date2;
      
      // Remove the time parts:
      // - eg: 11/08/2004 17:15:18 becomes 11/08/2004 00:00
                  
      time1 = GetDateTimeDate(date1);
      time2 = GetDateTimeDate(date2);
      
      // Now reset these values to the beginning of their respective weeks
      // eg: 11/08/2004 (Wed) becomes 09/08/2004 (Mon)
      
      time1 = GetDateTimeDate(time1) - ((24*60*60) * (Weekday(time1)-1));
      time2 = GetDateTimeDate(time2) - ((24*60*60) * (Weekday(time2)-1));
      
      // - Now both dtmfrom and dtmto are set to Monday on the start of their respective weeks.
      // - Now return how many 7-day weeks are between the 2:      
      return (time2-time1)/(7*24*60*60);
    }
    else if (interval == "h") {
      // Hour
      return  (date2-timezone)/(60*60) - (date1-timezone)/(60*60);
    }
    else if (interval == "n") {
      // Minute
      return (date2-timezone)/60 - (date1-timezone)/60;      
    }
    else if (interval == "s") {
      // Second
      return (date2-date1);
    }
    else {
      // Bad interval
      rr_throw("Unknown interval string: \"" + interval + "\"");
    }

    // If execution reaches here we have a problem!
    rr_throw("Logic error!");
  }

  tm datetime_to_tm(DateTime Time) {
    // Convert time_t representation (seconds) to tm (structured record) representation
    tm ret;
    tm * pret = localtime_r(&Time, &ret);

    // Was localtime_r successful?
    if (pret != &ret) {
      rr_throw("localtime_r() failed. DateTime value: " + ltostr(Time));
    }

    // Return the result:
    return(ret);
  }

  string format_datetime(const DateTime datetime, const string & strFormat) {
    // Format the time "Time" into a destination string, using
    // VB-like formatting rules (in strFormat) to do so
    // - Except a % sign is required in front - eg to get "year, month, day"
    // - The original vb would be "yyyy, mmmm, dd, but this is now changed to:"
    //	"%yyyy, %mmmm, %dd"

    string strDest = ""; // This string is returned..

    // Strings to describe month numbers and wee-day numbers listed in "tm" records.
    string ShortMonthNames[12] =
      {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    string LongMonthNames[12] =
      {"January","February","March","April","May","June","July","August","September","October","November", "December"};
    string ShortWeekDayName[7] =
      {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

    string LongWeekDayName[7] =
      {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};

    // Convert DateTime datetime to tm format
    tm Time = datetime_to_tm(datetime);

    // Copy over the formatting string
    strDest = strFormat;

    // Now perform replacements based on the contents of the formatting string;

    // First of all build strings based on the numeric parts of the date - this is so we
    // Have the formatted 0s (eg "3 = 03") where we want them.
    char strHour2[3];
    char strSec2[3];
    char strMin2[3];
    char strMDay2[3];
    char strMonth2[3];
    char strYear4[5];

    // Find out whether to use the 12-hour or 24-hour time representation
    bool Use12HourRep = (strDest.find("am/pm") != strDest.npos) ||
        (strDest.find("AM/PM") != strDest.npos) ||
        (strDest.find("hh") != strDest.npos) ||
        (strDest.find("t") != strDest.npos);

    if (Use12HourRep && Time.tm_hour > 12)
      sprintf(strHour2, "%02d", Time.tm_hour - 12);
    else
      sprintf(strHour2, "%02d",Time.tm_hour);

    // If the hour is "00" then list as 12
    if (strHour2 == "00") strcpy(strHour2, "12");

    // Format the rest of the time values
    sprintf(strSec2, "%02d", Time.tm_sec);
    sprintf(strMin2, "%02d", Time.tm_min);

    sprintf(strMDay2, "%02d", Time.tm_mday);
    sprintf(strMonth2, "%02d", Time.tm_mon + 1);
    sprintf(strYear4, "%02d", 1900 + Time.tm_year);

    // Now do the replacements

    // Times

    strDest = replacei(strDest, "%HH", strHour2);
    strDest = replacei(strDest, "%H", strHour2[0]=='0'?(char *)&strHour2[1]:strHour2);
    strDest = replacei(strDest, "%NN", strMin2);
    strDest = replacei(strDest, "%SS", strSec2);
    strDest = replace(strDest, "%s", strSec2[0]=='0'?(char*)&strSec2[1]:strSec2);

    strDest = replace(strDest, "%am/pm", Time.tm_hour>=12?"pm":"am");
    strDest = replace(strDest, "%AM/PM", Time.tm_hour>=12?"PM":"AM");
    strDest = replacei(strDest, "%tt", Time.tm_hour>=12?"PM":"AM");
    strDest = replacei(strDest, "%t", Time.tm_hour>=12?"A":"P");

    // Dates
    strDest = replacei(strDest, "%yyyy", strYear4);
    strDest = replacei(strDest, "%yyy", strYear4);
    strDest = replacei(strDest, "%yy", &strYear4[2]);
    strDest = replacei(strDest, "%y", &strYear4[3]);

    strDest = replacei(strDest, "%MMMM", LongMonthNames[Time.tm_mon]);
    strDest = replacei(strDest, "%MMM", ShortMonthNames[Time.tm_mon]);
    strDest = replacei(strDest, "%MM", strMonth2);

    string strReplace = strMonth2[0]=='0'?(char *)&strMonth2[1]:strMonth2;

    strDest = replacei(strDest, "%M", strReplace);

    strDest = replacei(strDest, "%DDDD", LongWeekDayName[Time.tm_wday]);
    strDest = replacei(strDest, "%DDD", ShortWeekDayName[Time.tm_wday]);
    strDest = replacei(strDest, "%DD", strMDay2);

    strReplace = strMDay2[0]=='0'?(char *)&strMDay2[1]:strMDay2;
    strDest = replacei(strDest, "%D", strReplace);

    // Return the result
    return strDest;
  }

  void GetDateParts(const DateTime dtmDate, int & intYear, int & intMonth, int & intDay) {
    // Convert DateTime datetime to tm format

    tm Time  = datetime_to_tm(dtmDate);
    intYear   = 1900 + Time.tm_year;
    intMonth = Time.tm_mon + 1;
    intDay  = Time.tm_mday;
  }

  DateTime GetDateTimeDate(DateTime datetime) {
    // Return the DateTime value, but without the time part
    return ((datetime-timezone) / (60*60*24))*(60*60*24) + timezone;
  }

  DateTime GetDateTimeTime(DateTime datetime) {
    // Return the DateTime value, but without the date part
    return (datetime-timezone) % (60*60*24) + timezone;
  }

  bool IsDate(const string & strDate) {
    // Check whether strDate is a valid representation of a date
    try {
      ParseDateString(strDate);
      // No exception, strDate is a valid date string.
      return true;
    }
    catch(...) {
      // ParseDateString threw an exception, not a valid date string!
      return false;
    }
  }                                                      

  bool IsTime(const string & strTime) {
    // Check whether strTime is a valid representation of a time
    try {
      ParseTimeString(strTime);
      return true; // ParseTimeString did not throw an exception, this is a valid time string
    }
    catch(...) {
      return false; // ParseTimeString threw an exception, this is an invalid time string.
    }
  }

  bool IsDateTime(const string & strDateTime) {
    // Check whether strTime is a valid representation of a time
    try {
      ParseDateTimeString(strDateTime);      
      return true; // ParseDateTimeString did not throw an exception, this is a valid date/time string.
    }
    catch(...) {
      // ParseDateTimeString threw an exception, this is an invalid date/time string!
      return false;
    }
  }

  DateTime MakeDateTime(int year, int month, int day, int hour, int minute, int second) {
    // Check the date - libc's mktime won't check for errors...

    // Check the year:
    if (year < 1970) {
      rr_throw("Bad year " + itostr(year) + " - year must be 1970 or later");
    }

    // Check the month:
    if ((month < 1) || (month > 12)) {
      rr_throw("Bad month " + itostr(month) + " - month must be between 1 and 12");
    }

    // Check the day:
    int month_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    // Is the year a leap year?
    if (is_leap_year(year)) month_days[1] = 29;
    if ((day < 0) || (day > month_days[month - 1])) {
      rr_throw("Bad day of month " + itostr(day) + " - day must be between 1 and " + itostr(month_days[month - 1]));
    }

    // Check the hour:
    if ((hour) < 0 || (hour > 23)) {
      rr_throw("Bad hour " + itostr(hour) + ". Hour must be between 0 and 23.");
    }

    // Check the minute:
    if ((minute < 0) || (minute > 59)) {
      rr_throw("Bad minute " + itostr(minute) + ". Minute must be between 0 and 59" );
    }

    // Check the second:
    // Check the minute:
    if ((second < 0) || (second > 59)) {
      rr_throw("Bad second " + itostr(second) + ". Second must be between 0 and 59" );
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
    DateTime dtmret = mktime(&tm_time);

    // -1 is returned if mktime failed.
    if (dtmret == -1) {
      // Throw an error:
      rr_throw("logic error - mktime call failed.");
    }
    return dtmret;
  }

  DateTime Now() {
    // Return the current date/time
    time_t curr;
    time(&curr);
    return (curr);
  }

  DateTime Time() {
    // Return the current Time (with no Date info)
    return GetDateTimeTime(Now());
  }

  int Weekday(const DateTime datetime) {
    // Based on the VB Weekday() function:
    // Assume Monday = day1, Sunday = day7
    tm tm_datetime = datetime_to_tm(datetime);

    int day = tm_datetime.tm_wday;
    if (day==0) day = 7;             // Convert Sun-Sat(0-6) to Mon-Sun(1-7)
    return day;
  }

  DateTime ParseDateString(const string & strDate) {
    // Take a simple string of the layout dd/mm/yyyy and convert it to a DateTime var
    const char * DateStr = strDate.c_str();
    if (!(isdigit(DateStr[0]) && isdigit(DateStr[1]) && DateStr[2] == '/' &&
        isdigit(DateStr[3]) && isdigit(DateStr[4]) && DateStr[5] == '/' &&
        isdigit(DateStr[6]) && isdigit(DateStr[7]) && isdigit(DateStr[8]) && isdigit(DateStr[9]))) {
      rr_throw("Bad date string");      
    }

    int intDay = strtoi(substr(strDate, 0, 2));
    int intMonth = strtoi(substr(strDate, 3, 2));
    int intYear = strtoi(substr(strDate, 6, 4));

    DateTime dtmDate = MakeDate(intYear, intMonth, intDay);
    string strTestDate = format_datetime(dtmDate, "%dd/%mm/%yyyy");
    if (strTestDate != strDate) {
      rr_throw("Date string failed a test!");
    }

    // Now return the parsed date:
    return dtmDate;      
  }

  DateTime ParseTimeString(const string & strTime) {
    // Take a simple string of the layout "xx:xx:xx am/pm" and convert it to a DateTime var.
    // - All fields after the first 2 xx groups are optional.
    DateTime dtmReturn = datetime_error;

    string strwork = trim(StringToLower(strTime));
    if (strwork.length() < 5)  {
      rr_throw("Time string is too short!");
    }
    
    // We have enough characters for a "hh:nn" time.
    const char * c = strwork.c_str();
    
    // Check the format of these 5 chars.
    if (!(isdigit(c[0]) && isdigit(c[1]) && (c[2] == ':') && isdigit(c[3]) && isdigit(c[4]))) {
      rr_throw("Bad time format in the first 5 chars!");
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
        rr_throw("6th character is not ':'");
      }

      // The 6th character is a ':' check for 2 more digit characters
      if (strwork.length() < pos + 4) {
        // There was a 2nd ':' character, but not enough characters after.
        rr_throw("Not enough characters after the 2nd ':'");
      }
      
      // 2 more characters found. Their type?
      if (!(isdigit(c[pos + 2]) && isdigit(c[pos + 3])))  {
        rr_throw("Invalid 'seconds' characters found!");
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
            rr_throw("Invalid characters found in the am/pm part of the time string");
          }
        }
        else {
          // There are additional characters, but not the correct amount!
          rr_throw("Bad number of characters found in the am/pm part of the time string");
        }
      } // otherwise, there is no AM/PM section.
    } // END: if (strwork.length() > (pos + 1)) // Check for more characters after the minute.

    // Check the range of the extracted numbers...
    // if an "am" or "pm" value was specified, then the hour cannot be more than 12...
    int intMaxHour = (strAMPM == "") ? 23 : 12;
    if (!((intNum1 >= 0) && (intNum1<= intMaxHour) && // Hour
        (intNum2 >= 0) && (intNum2 <= 59) && // Minutes
        (intNum3 >= 0) && (intNum3 <= 59))) {
      rr_throw("Hours, minutes, or seconds are out of range.");
    }

    // Now piece together the extracted parts, and make the date to return.
    // - Generate the base time
    dtmReturn = MakeTime(intNum1, intNum2, intNum3);
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

  DateTime ParseDateTimeString(const string & strDateTime) {
    // Take string var, see if it has date or time portions, and attempt to extract the portions.
    // The acceptible layouts are: "[date]", "[date] [time]" and "[time] [date]" where
    // [date] is in the format "dd/mm/yyyy" and time is in the format "hh:nn:ss ?m". However,
    // with [time], the ":ss" and "?m" parts are optional (ps: ?m means AM or PM)

    DateTime dtmRet = datetime_error; // Reset the return result...

    // Pull the date/time sections out of the string...
    string strTime = "";
    string strDate = "";

    // Is there a date portion? Search for the first "/" character
    string strwork = StringToLower(trim(strDateTime));
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
      rr_throw("Could not extract a date or time!");
    }

    // We now have any date and time portions extracted from their various positions within
    // the string. Now attempt to convert to a datetime.
    DateTime dtmDate = datetime_error;
    DateTime dtmTime = datetime_error;

    // Was a date string extracted?
    if (strDate != "") {
      dtmDate = ParseDateString(strDate); // This will throw an exception if parsing fails...
      dtmRet = dtmDate;
    }

    // Was a time string extracted?
    if (strTime != "") {
      dtmTime = ParseTimeString(strTime); // This will throw an exception if parsing fails
      dtmRet = dtmTime;
    }

    // Did we extract both a date and time string?
    if ((strDate != "") && (strTime != "")) {
      // Add the 2 parsed times together:
      dtmRet = dtmDate + dtmTime - timezone; // Add the time and subtract the timezone variable (ie: datetime maths)
    }

    return dtmRet;
  }

  /***************************************************************************
   *   String                                                                *
   ***************************************************************************/

  // String conversions:
  // - Boolean conversion:
  bool strtobool(const string & str) {
    // Convert a string to a boolean, throw an exception if there is an error.

    string strbool = StringToLower(trim(str)); // Fetch, trim and conver the arg to lowercase.

    // Return the appropriate true or false value depending on the string contents:
    // - Checks sorted in the order of probability...
    if (strbool == "true") return true;
    if (strbool == "false") return false;
    if (strbool == "1") return true;
    if (strbool == "0") return false;
    if (strbool == "yes") return true;
    if (strbool == "no") return false;    
    if (strbool == "y") return true;
    if (strbool == "n") return false;

    // Cannot determine if strbool represents a "true" or "false" value!
    rr_throw("Cannot convert \"" + str + "\" to a boolean value!");
  }

  bool IsBool(const string & strBool) {
    // Returns true if the string represents a valid boolean value.
    try {
      strtobool(strBool); 
      return true;
    }
    catch(...) {
      return false;
    }
  }

  string booltostr(bool blnval) {
    // Return a string representation of a boolean value.
    return (blnval?"true":"false");
  }

  // - Numeric conversion:
  //   - Int:
  int strtoi(const string & str) {
    // Convert a string var to an int. Throw exceptions if there are errors.

    // Make a local copy of the string, and trim it:
    string strnumber = trim(str);

    // Check for emptiness
    if (strnumber == "") {
      rr_throw("Cannot convert an empty string to an integer!");
    }

    // Convert to a long:
    char * TailPtr; // *Tailptr points to the character after the last processed
                            // character. If the integer has no problems then *TailPtr should be '\0'
    long lngNum = std::strtol(strnumber.c_str(), &TailPtr, 10);

    // Check if the entire string was parsed:
    if (*TailPtr != 0) {
      rr_throw("Could not convert \"" + strnumber + "\" to an integer");
    }

    // Check if the converted number is in range for an int (we converted to a long because there
    // is no strtoi function in libc:
    if ((lngNum <= INT_MIN) || (lngNum >= INT_MAX)) {
      rr_throw(ltostr(lngNum) + " is outside of the allowed integer range!");
    }

    // Error checks are done. Return the integer:
    return lngNum;
  }
  
  bool IsInt(const string & strNumber) {
    // Return true if the string is a valid integer...
    try {
      strtoi(strNumber);
      return true;
    }
    catch(...) {
      return false;
    }
  }

  string itostr(const int num) {
    // Convert int to char array
    char conv_num[256];
    sprintf(conv_num, "%d", num);
    return conv_num;
  }
  
  //   - Long:
  long strtol(const string & str) {
    // Convert a string var to a long. Throw exceptions if there are errors.

    // Make a local copy of the string, and trim it:
    string strnumber = trim(str);

    // Check for emptiness
    if (strnumber == "") {
      rr_throw("Cannot convert an empty string to a long integer!");
    }

    // Convert to a long:
    char * TailPtr; // *Tailptr points to the character after the last processed
                            // character. If the integer has no problems then *TailPtr should be '\0'
    long lngNum = std::strtol(strnumber.c_str(), &TailPtr, 10);

    // Check if the entire string was parsed:
    if (*TailPtr != 0) {
      rr_throw("Could not convert \"" + strnumber + "\" to a long integer!");
    }

    // Check if the converted number is in the allowed range:  
    if ((lngNum <= LONG_MIN) || (lngNum >= LONG_MAX)) {      
      rr_throw(strnumber + " is outside of the allowed long integer range!");
    }

    // Error checks are done. Return the long integer:
    return lngNum;
  }
  
  bool IsLong(const string & strNumber) {
    // Return true if the string represents a valid long int.
    try {
      strtol(strNumber);
      return true;
    }
    catch(...) {
      return false;
    }
  }
   
  
  string ltostr(const long num) {
    // Convert long to char array
    char conv_num[256];
    sprintf(conv_num, "%ld", num);
    return conv_num;
  }
  
  //   - Unsigned Long Long:
  unsigned long long int strtoull(const string & str) {
    // Convert a string var to an unsigned long long

    // Make a local copy of the string, and trim it:
    string strnumber = trim(str);

    // Check for emptiness
    if (strnumber == "") {
      rr_throw("Cannot convert an empty string to an unsigned long long integer!");
    }

    // Convert to a long:
    char * TailPtr; // *Tailptr points to the character after the last processed
                            // character. If the integer has no problems then *TailPtr should be '\0'
    unsigned long long lngNum = std::strtoull(strnumber.c_str(), &TailPtr, 10);

    // Check if the entire string was parsed:
    if (*TailPtr != 0) {
      rr_throw("Could not convert \"" + strnumber + "\" to an unsigned long long integer!");
    }

    // Check if the converted number is in the allowed range:
    if ((lngNum >= ULONG_LONG_MAX)) {
      rr_throw(strnumber + " is outside of the allowed unsigned long long integer range!");
    }

    // Error checks are done. Return the long integer:
    return lngNum;
  }
  
  bool IsUnsignedLongLong(const string & strNumber) {
    // Return true if the string represents a valid unsigned long long integer.
    try {
      strtoull(strNumber);      
      return true;
    } catch(...) {
      return false;
    }
  }

  //   - Double:
  double strtod(const string & str) {
    // Convert a string var to a double. Throw an exception if there is a problem.

    // Make a local copy of the string, and trim it:
    string strnumber = trim(str);

    // Check for emptiness
    if (strnumber == "") {
      rr_throw("Cannot convert an empty string to a double!");
    }

    // Convert to a long:
    char * TailPtr; // *Tailptr points to the character after the last processed
                            // character. If the integer has no problems then *TailPtr should be '\0'
    double dblNum = std::strtod(strnumber.c_str(), &TailPtr);

    // Check if the entire string was parsed:
    if (*TailPtr != 0) {
      rr_throw("Could not convert \"" + strnumber + "\" to a double!");
    }

    // Error checks are done. Return the double;
    return dblNum;
  }
  
  bool IsDouble(const string & strNumber) {
    try {
      strtod(strNumber);
      return true;
    }
    catch(...) {
      return false;
    }
  }

  // Other string functions:

  string Left(const string & str, const int left_chars) {
    // Return the first [left_chars] characters of a string
    return substr(str, 0, left_chars);
  }

  string ensure_char_at_end(const string & str, const char AtEnd) {
    string strret=str;
    // Check if a given char is at the end of a string. If it is not there then append it.
    // useful for making sure that paths have a '\' on the end.
    if (strret.length() > 0) {
      if (strret[(strret.length())-1] != AtEnd) {
        strret += AtEnd;
      }
    }
    return strret;
  }

  string GetShortFileName(const string & strLongFileName) {
    // Remove all characters up to and including the first slash / , return.
    // Find the last "/" character
    string strShortFileName = strLongFileName;

    unsigned Pos = strLongFileName.rfind("/");
    if (Pos == string::npos)
      // Couldn't find the last slash - return the string unmodified
      return strShortFileName;

    // Return the filename after this point
    strShortFileName = substr(strShortFileName, Pos + 1, strShortFileName.length());
    return strShortFileName;
  }


  string remove_ending_cr(const string & str) {
    // This function should be used when reading in text files that were created under DOS/Windows. DOS/Windows ends
    // each file line with CR/LF (2 charaters - Carriage Return & Line Feed #13#10), while Unix/Linux ends each
    // line with just LF - so when linux reads in lines from a text file created under Windows, there will be a trailing
    // CR character. Call this function after reading in a string to remove this trailing CR character if it exists
    string strret = str;
    if (Right(strret, 1) == "\r")
      // Shorten the string by one character
      strret = strret.substr(0, strret.length()-1);
    return strret;
  }

  string replace(const string & strSearchIn, const string & strSearchFor, const string & strReplaceWith, const bool CaseSensitive) {
    //
    // * CaseSensitive has a default value of true
    //
    // Search for and replace all occurances of one sub-string with another string.
    string Temp_SearchIn = strSearchIn.c_str(); // C++ = wierd -> this is needed
    string Temp_SearchFor = strSearchFor;

    if (!CaseSensitive) {
      Temp_SearchIn = StringToLower(Temp_SearchIn);
      Temp_SearchFor = StringToLower(Temp_SearchFor);
    }

    int SearchForLen = strSearchFor.length();

    int ReplaceWithLen = strReplaceWith.length();
    unsigned Pos = Temp_SearchIn.find(Temp_SearchFor, 0);

    string strret=strSearchIn;
    while (Pos!=Temp_SearchIn.npos) {
      // Replace in both the SearchIn String and the case-(in)sensitive one.
      strret.replace(Pos, SearchForLen, strReplaceWith);
      Temp_SearchIn.replace(Pos, SearchForLen, strReplaceWith);
      // Find the next postition to replace at
      Pos = Pos + ReplaceWithLen;
      Pos = Temp_SearchIn.find(Temp_SearchFor, Pos);
    }
    return strret;
  }

  string replacei(const string & strSearchIn, const string & strSearchFor, const string & strReplaceWith) {
    // Call Replace with a false CaseSensitive value
    return replace(strSearchIn, strSearchFor, strReplaceWith, false);
  }

  string Right(const string & str, const int right_chars) {
    // Return a string with characters from the right end of the string
    return substr(str, str.length()-right_chars, right_chars);
  }

  string StringToLower(const string & str) {
    // Convert a string to lower case
    string strRet = str;
    int intLen = strRet.length();

    for(int i=0; i<intLen; i++)
      strRet[i] = tolower(strRet[i]);

    return strRet;
  }

  string StringToUpper(const string & str) {
    // Convert a string to upper case
    string strRet = str;
    int intLen = strRet.length();

    for(int i=0; i<intLen; i++)
      strRet[i] = toupper(strRet[i]);

    return strRet;
  }

  char * strstri(const string & SearchIn, const string & SearchFor) {
    // My custom version of strstr that performs a case insensitive search;
    string TempIn, TempFor;

    TempIn = SearchIn;
    TempFor = SearchFor;

    TempIn = StringToUpper(TempIn)	;
    TempIn = StringToUpper(TempFor);

    // return either NULL if the search result was NULL, or return a pointer to the position
    // in the original string where the substring was found
    char * Pos = strstr(TempIn.c_str(), TempFor.c_str());
    return Pos==NULL?NULL:(char *)((unsigned)SearchIn.c_str()+(unsigned)Pos-(unsigned)TempIn.c_str());
  }

  string substr(const string & str, const int startpos, const int len) {
    // My custom version of the substr function - because if you use the string.substr function with an
    // invalid startpos (or len) value it raises an exception. This procedure returns an empty srting ""
    // instead.
    try {
      return str.substr(startpos, len);
    }
    catch(...) { return ""; }
  }

  string substr(const string & str, const int startpos) {
    // My custom version of the substr function - because if you use the string.substr function with an
    // invalid startpos (or len) value it raises an exception. This procedure returns an empty sting ""
    // instead.
    try {
      return str.substr(startpos, str.length());
    }
    catch(...) { return ""; }
  }

  string trim(const string & strStringToTrim) {
    // Remove spaces from the start and the end of a passed string
    string strRetStr = strStringToTrim;

    // Remove from the front:
    string strchar = substr(strRetStr, 0, 1);
    while (strchar == " " || strchar == "\n") {
      strRetStr = substr(strRetStr, 1, strRetStr.length());
      strchar = substr(strRetStr, 0, 1);
    }

    // Remove from the end:
    strchar = substr(strRetStr, strRetStr.length() - 1, 1);
    while (strchar == " " || strchar == "\n") {
      strRetStr = substr(strRetStr, 0, strRetStr.length()-1);
      strchar = substr(strRetStr, strRetStr.length() - 1, 1);
    }  
    return strRetStr;
  }

  string WrapLines(const string & strText, const long lngSpaceAfter, const string & strNextLineTabChars) {
    // Take a possibly very long string in strText and wrap the lines.
    // After each (lngSpaceAfter) characters go onto a new line when a
    // space character is encountered. At the start of the next line
    // insert [strNextLineTabChars] - this is to allow paragraph formatting.

    int intCharsOnLine = 0; // Characters output on the current line in the output string.
    string strReturn = "";  // The final formatted string to return.
    
    for (long lngPos = 0; lngPos < (int)strText.length(); ++lngPos) {
      ++intCharsOnLine;
      string strChar = substr(strText, lngPos, 1);
      strReturn += strChar;
      if ((intCharsOnLine >= lngSpaceAfter) && (strChar == " ")) {
        strReturn += "\n" + strNextLineTabChars;
        intCharsOnLine = 0;
      }
    }
    return strReturn;
  }

  /***************************************************************************
  *   Misc base conversion                                                   *
  ****************************************************************************/
  unsigned long Base36ToDec(const string & strBase36) {
    // Base36 is a number system that runs from 0-9 through A-Z. This
    // function converts a base36 value to decimal and returns the value
    string strnum = StringToUpper(strBase36);

    unsigned long lngPlaceValue = 1;
    unsigned long lngReturnVal = 0;

    while (strnum != "") {
      string strChar = Right(strnum, 1);
      int intDigitVal;
      if (IsInt(strChar)) {
        intDigitVal = strtoi(strChar);
      }
      else {
        intDigitVal = 10 +  strChar[0] - 'A';
      }
      lngReturnVal += intDigitVal * lngPlaceValue;
      lngPlaceValue *= 36;
      strnum = substr(strnum, 0, strnum.length() - 1);
    }
    return lngReturnVal;
  }

  string DecToBase36(const unsigned long lngDecVal, const int intPlaces) {
    // Converts a decimal number to Base36. Pads up to intPlaces with
    // 0's
    string strRetVal = "";
    long lngnum = lngDecVal;
    while (lngnum > 0) {
      unsigned long intPlace = lngnum % 36;
      if (intPlace < 10) {
        strRetVal =  itostr(intPlace) + strRetVal;
      }
      else {
        char ch [3] = "\0\0";
        ch[0] = 'A' + intPlace - 10;
        strRetVal = ch + strRetVal;
      }
      lngnum = lngnum / 36;
    }

    while (strRetVal.length() < (unsigned) intPlaces) {
      strRetVal = "0" + strRetVal;
    }
    return strRetVal;
  }

  /***************************************************************************
   *   Directory and file-handling                                           *
   ***************************************************************************/
  void AppendStringToTextFile(const string & strString, const string & strFilePath) {
    // Attempt to open a text file in append mode, append a string, and then close the text file.
    ofstream TextFile;
    TextFile.open(strFilePath.c_str(), ios::out | ios::app);
    if (TextFile) {
      // Logfile opened, write the log
      TextFile << strString << endl;
      TextFile.close();
    } // end if
    else {
      rr_throw("Could not open file " + strFilePath + " for appending!");
    }
  }

  void break_down_file_path(const string & strfilepath, string &strfile_dir, string &strfile_name) {
    // Take the path of a file, and return the directory and filename of the file
    // Find the right-most slash
    unsigned intpos = strfilepath.rfind("/");
    if (intpos ==string::npos) {
      // No slash found. Assume that the entire string is a filename
      strfile_dir = "";
      strfile_name = strfilepath;
    }
    else {
      // A slash was found. Return the string before (and including), and the string after this point,
      // as the directory name and the file name respectively.
      strfile_dir = substr(strfilepath, 0, intpos + 1);
      strfile_name = substr(strfilepath, intpos + 1);
    }
  }

  void cp(const string & strsource_arg, const string & strdest_arg) {
    // Copy a file from a source to a destination. Use an intermediate ".ITF" filename.
    // If there are any errors then log an error and throw an exception.

    // Fetch the args into local variables:
    string strsource = strsource_arg;
    string strdest   = strdest_arg;
    
    // Check that the source file exists before copying:
    if (!FileExists(strsource)) {
      rr_throw("Copy failed, file not found: " + strsource);
    }

    // If the destination is a directory (or there is a slash on the end),
    // then add the source filename to the end:
    if (DirExists(strdest) || (Right(strdest, 1) == "/")) {
      // Specified destination is an existing directory. Concatenate the base filename
      // from the source file:
      string strsource_dir="";
      string strsource_file="";
      break_down_file_path(strsource, strsource_dir, strsource_file);
      strdest = ensure_char_at_end(strdest, '/') + strsource_file;
    }

    // Fetch absolute path versions of the args:
    strsource = RelPathToAbs(strsource, PATH_IS_FILE);
    strdest   = RelPathToAbs(strdest, PATH_IS_FILE);

    // Now concatenate ".ITF" for the temporary filename:
    string strtemp_dest = strdest + ".ITF";

    // Generate and run a command to copy to .ITF:
    string strcmd = "cp " + string_to_unix_filename(strsource) + " " + string_to_unix_filename(strtemp_dest);
    string strcmd_out = ""; // Output of the command.

    if (system_capture_out(strcmd, strcmd_out) != 0) {
      // File copy failed!
      // - Remove the .ITF file if it exists:
      if (FileExists(strtemp_dest)) {
        if (remove(strtemp_dest.c_str()) != 0) {
          // Remove failed!
          rr_throw("Removal of temporary file " + strtemp_dest + " failed!");
        }
      }
      // - Now log an error and throw an exception:
      rr_throw("Copy failed! Reason: " + strcmd_out);
    }

    // File copy succeeded (source -> temp destination). Now attempt to move the temp destination to the final destination.
    if (rename(strtemp_dest.c_str(), strdest.c_str()) != 0) {
      // Rename from filename.ext.ITF to filename.ext failed!
      // - Remove the temporary .ITF file:
      if (remove(strtemp_dest.c_str()) != 0) {
        // Remove failed!
        rr_throw("Removal of temporary file " + strtemp_dest + " failed!");
      }
      // - Now log an error and throw an exception:
      rr_throw("Could not rename " + strtemp_dest + " to " + strdest + "!");
    }
    // The copy succeeded. There were no exceptions thrown.
  }

  void ClearReadOnlyInDir(const string & strpath) {
    // Clear the readonly attribute of all files and subfolders in a directory
    if (DirExists(strpath)) { // Prevent "funny" paths from being run with system()
      string cmd = "chmod 777 -R " + strpath;
      system(cmd.c_str());
    }
  }

  long CountDirFiles(const string & strpath) {
#ifndef __linux__
    undefined_func_throw;
#else
    // Count how many files are in a directory and return this result
    int lngNumFiles = 0;
    dir_list Dir(strpath);

    string strFile = Dir.item();
    while (strFile != "") {
      lngNumFiles++;
      strFile = Dir.item();
    }
    return lngNumFiles;
#endif    
  }

  long CountFileLines(const string & strfilepath) {
    // Count File Lines
    ifstream inFile(strfilepath.c_str());

    if (!inFile) {
      // Unable to open file - return -1
      return -1;
    }
    else {
      char ch_FileLine[2048] = "";
      int LineCounter=0;
      while (inFile.getline(ch_FileLine, 2047))
        LineCounter++; // Update the line count
      return LineCounter;
    }
  }

  void df(const string & strfile, unsigned long long int & total, unsigned long long int & used, unsigned long long int & available, string & strfilesystem, string & strmounted_on) {
    // Shell a "DF" call, parse the output, and returns the following information
    // about the filesysem the file (or directory) is mounted on:
    //   A) The total space of the filesystem
    //   B) The used space of the filesystem
    //   C) The total available space
    //   D) The filesystem the specified file (or directory) is on.

    // Reset the args:
    total = used = available = 0;
    strfilesystem = strmounted_on = "";

    // Prepare for a DF call:
    string strcmd="/bin/df -P -B 1 \"" + strfile + "\"";
    string strcmd_out="";

    // Run and check the DF call result:
    if (system_capture_out(strcmd, strcmd_out) != 0) {
      rr_throw(strcmd_out);
    }

    // No problem with the DF call, so parse the output
    // eg output:
    //   Filesystem            1-blocks      Used Available Use% Mounted on
    //   /dev/hda3            9844887552 5526700032 3818090496  60% /

    string strline;
    {
      string_splitter split(strcmd_out, "\n", "!!");
      split.next(); // Throw away the first line (we want the 2nd line);
      strline=split; // Get the 2nd line
      string strEND=split; // Check that we only got 2 lines

      if (strEND != "!!") {
        rr_throw("Input output from df! I expected 2 lines!");
      }
    }

    // Now split the 2nd line into 6 parts:
    string_splitter split (strline, " ", "!!");
    
    // - Filesystem
    strfilesystem=split;
    
    // - 1-blocks (size in bytes)
    total=strtoull(split);
    // - Use
    used=strtoull(split);
    // - Available
    available=strtoull(split);

    split.next(); // Ignore the 5th element - this is DF's usage output...
   
    // - Mounted on
    strmounted_on=split;

    // Check that we found the correct # of fields:
    if ((string)split != "!!") {
      rr_throw("An error parsing DF's output! I expected 6 parts!");
    }
  }

  bool DirExists(const string & strPath) {
#ifndef __linux__
    undefined_func_throw;
#else  
    // Return true if the directory exists, otherwise return false.
    DIR *dir_p;

    // The opendir function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
    string strFriendlyPath = RelPathToAbs(strPath, PATH_IS_DIR);

    dir_p = opendir(strFriendlyPath.c_str());
    bool Exists = dir_p!=NULL;
    closedir(dir_p);

    return Exists;
#endif    
  }

  bool FileExists(const string & strPath) {
    // Return true if the file exists, otherwise return false. Directories and character
    // devices are not considered as "files"
    struct stat stat_p;
    bool Ret_Val = false;

    // The stat function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
    string strStatFriendlyPath = strPath;

    if (strStatFriendlyPath.length() > 0)
      if (strStatFriendlyPath[0] == '~')
        strStatFriendlyPath = getenv("HOME") + string(strStatFriendlyPath, 1);

    if (stat(strStatFriendlyPath.c_str(), &stat_p) != -1)
      Ret_Val = S_ISREG(stat_p.st_mode);  // Only return true if a "regular" file

    return Ret_Val;
  }

  bool FileExists_CaseInsensitive(const string & strDir, const string & strFileName, string & strActualFileName) {
#ifndef __linux__
    undefined_func_throw;
#else
    // Returns true if a file with a matching filename exists in the folder. A file in a folder matches, if
    // it's filename is the same as the filename being searched for. The search is case insensitive.
    strActualFileName = "";
    bool blnFound = false;
    dir_list Dir(strDir);
    string strDirFile = Dir.item();

    // First do this the easy way - does the file exist? (case sensitive)
    if (FileExists(strDir + strFileName)) {
      strActualFileName = strFileName;
      return true;
    }

    // Not found - do a case-insensitive search
    string strLowerFName = StringToLower(strFileName);

    while (!blnFound && strDirFile != "") {
      string strTemp;

      char strOldDirFile[1024];
      strcpy(strOldDirFile, strDirFile.c_str());

      strTemp = strDirFile;
      strTemp = StringToLower(strTemp);
      if (strTemp == strLowerFName) {
        blnFound = true;
        strActualFileName = strOldDirFile;
      }
      strDirFile = Dir.item();
    }

    return blnFound;
#endif    
  }

  long FileSize(const string & strPath) {
    // Return the filesize if the file exists, otherwise return -1
    struct stat stat_p;
    long Ret_Val = -1;

    // The stat function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
    string strStatFriendlyPath = strPath;

    if (strStatFriendlyPath.length() > 0)
      if (strStatFriendlyPath[0] == '~')
        strStatFriendlyPath = getenv("HOME") + string(strStatFriendlyPath, 1);

    if (stat(strStatFriendlyPath.c_str(), &stat_p) != -1)
      if S_ISREG(stat_p.st_mode)
        Ret_Val = stat_p.st_size;

    return Ret_Val;
  }

  bool FindTextInFile(const string & strText, const string & strFilePath) {
    string strCommand = string("/bin/grep \"") + strText + "\" \"" + strFilePath + "\"" + " &> /dev/null";
    strCommand = replace(strCommand, "$", "\\$");
    return system(strCommand.c_str()) == 0;
  }

  string getcwd() {
    // Return the current working directory as a string object
    char *buffer = ::getcwd(NULL, 0);
    string strret = buffer;
    free(buffer);
    return strret;
  }

  string GetExecPath() {
  #ifdef __linux__
    // Return the path and executable that was run to start this process. eg /bin/xmms for XMMS.
    return ReadSymLink("/proc/" + itostr(getpid()) + "/exe");
  #else
    undefined_func_throw;
    return "";
//    rr_throw("Function not defined...");
  #endif  
  }

  string GetExecDir() { // Just the directory part of GetExecPath
  // An update for RR apps: Usually RR apps are run as binaries under a "binary" subdir, and are
  // pointed to by a symbolic link in the parent directory. In the case of RR software, we are
  // interested in the parent directory, *not* the "binary" repository.
    string strfile, strpath;
    break_down_file_path(GetExecPath(), strpath, strfile);
    strpath = ensure_char_at_end(strpath, '/');

    // And now remove the "binary" part at the end.
    if (StringToLower(Right(strpath,8)) == "/binary/") {
      strpath = substr(strpath, 0, strpath.length() - 8);
      strpath = ensure_char_at_end(strpath, '/');
    }

    return strpath;
  }

  DateTime GetFileDateModified(const string & strPath) {
    // Fetch the file modifed date/time
    struct stat file_stat;

    int fail = stat(strPath.c_str(), &file_stat);

    if(fail) {
      // could not access the file
      rr_throw("Could not access file " + strPath);
    }
    else {
      return file_stat.st_mtime;
    }
  }

  void mkdir(const string & Path) {
    // Create a directory and all it's parents as necessary
    if (trim(Path) == "") {
      rr_throw("Argument to mkdir is empty!");
    }
    
    string strcmd = "mkdir -p " + string_to_unix_filename(Path);
    string strcmd_out = "";    
    if (system_capture_out(strcmd, strcmd_out) != 0) {
      rr_throw(strcmd_out);
    }
  }

  void mv(const string & strsource, const string & strdest_arg) {
    // Move a file from a source to a destination. Use an intermediate ".ITF" filename.
    // If there are any errors then log an error and throw an exception.

    // Check that the source file exists before copying:
    if (!FileExists(strsource)) {
      rr_throw("Move failed, file not found: " + strsource);
    }

    // Process the "Destination" arg:
    string strdest = strdest_arg;

    // If the destination is a directory (or there is a slash on the end),
    // then add the source filename to the end:
    if (DirExists(strdest) || (Right(strdest, 1) == "/")) {
      // Specified destination is an existing directory. Concatenate the base filename
      // from the source file:
      string strsource_dir="";
      string strsource_file="";
      break_down_file_path(strsource, strsource_dir, strsource_file);
      strdest = ensure_char_at_end(strdest, '/') + strsource_file;
    }

    // Attempt to move the file to a temporary ".ITF" location:
    string strtemp_dest = strdest + ".ITF";
    string strcmd = "mv " + string_to_unix_filename(strsource) + " " + string_to_unix_filename(strtemp_dest);
    string strcmd_out = ""; // Output of the command.

    if (system_capture_out(strcmd, strcmd_out) != 0) {
      // File copy failed!
      // - Remove the .ITF file if it exists:
      if (FileExists(strtemp_dest)) {
        if (remove(strtemp_dest.c_str()) != 0) {
          // Remove failed!
          rr_throw("Removal of temporary file " + strtemp_dest + " failed!");
        }
      }
      // - Now log an error and throw an exception:
      rr_throw("Move failed! Reason: " + strcmd_out);
    }

    // File move succeeded (source -> temp destination). Now attempt to move the temp destination to the final destination.
    if (rename(strtemp_dest.c_str(), strdest.c_str()) != 0) {
      // Rename from filename.ext.ITF to filename.ext failed!
      // - Remove the temporary .ITF file:
      if (remove(strtemp_dest.c_str()) != 0) {
        // Remove failed!
        rr_throw("Removal of temporary file " + strtemp_dest + " failed!");
      }
      // - Now log an error and throw an exception:
      rr_throw("Could not rename " + strtemp_dest + " to " + strdest + "!");
    }
    // The move succeeded. There were no exceptions thrown.
  }

  string ReadSymLink(const string & strPath) {
#ifndef __linux__
    undefined_func_throw;
#else
    // Read the path that a symbolic link points to.

    // ...code adapted from the example in libc.txt...
    int size = 100;
    string strRet = "";
    bool blnquit=false; // Set to true when the loop must stop.

    do {
      char *buffer=(char *) xmalloc (size);
      int nchars = readlink (strPath.c_str(), buffer, size);
      if (nchars < 0) {
        // readlink had an error
        blnquit=true;
      } // end if
      else if (nchars < size) {
        // Readlink was successful.
        buffer[nchars] = 0; // - Make sure the string ends at the end of the number
        strRet = buffer; // Populate the return value
        blnquit = true; // No need to grow the buffer
      } // end else

      // Prepare the loop for the next iteration - free the buffer and make the next buffer size larger
      free (buffer);
      size *= 2;
    } while (!blnquit); // end while

    // Now return any Symbolic Link details that were read.
    return strRet;
#endif
  } // end function

  string RelPathToAbs(const string & strPath, const PATH_TYPE path_type) {
#ifndef __linux__
    undefined_func_throw;
#else
    // Converts a relative path to an absolute path.
    // (has no effect on paths which are already absolute)
    string strret = strPath;

    // First replace any starting $ with the the home path.
    if (strret.length() > 0) {
      if (strret[0] == '~') {
        strret = getenv("HOME") + string(strPath, 1);
      }
    }

    // Now use the stdlib library function for path resolution
    char actualpath [PATH_MAX+1];
    memset(actualpath, '\0', sizeof(actualpath));

  //  char *ptr = realpath(strPath.c_str(), actualpath);
    realpath(strret.c_str(), actualpath);
    if (actualpath!="") {
      strret = actualpath;
    }

    // Finally, if this absolute path refers to a directory, then make sure the last
    // character is a "/"
    if (path_type == PATH_IS_DIR) {
      strret = ensure_char_at_end(strPath, '/');
    }

    return strret;
#endif
  }

  void rm(const string & strfile) { // Throws an exception if the file remove fails.
    if (remove(strfile.c_str()) != 0) {
      rr_throw("rm: \"" + strfile + "\": " + strerror(errno));
    }
  }

  void rmdir(const string & strdir) { // Throws an exception if the rmdir fails.
    if (::rmdir(strdir.c_str()) != 0) {
      rr_throw("rmdir: \"" + strdir + "\": " + strerror(errno));
    }
  }

  string string_to_unix_filename(const string & strFName) {
    // Use this string to replace " " characters in filenames with "\ "
    return replace(strFName, " ", "\\ ");
  }

  /***************************************************************************
   *   Conf-file handling                                                    *
   ***************************************************************************/
  // Internal function used by xmms_controller::get_sound_latency()
  bool ifstream_find_line_starting_with(ifstream * input_file, const string & strStartText, string * strLine, const string & strErrorStartText) {
    // This function searches an input file stream for a line which starts with the specified text
    // (not case sensitive). The input stream is progressed to the line, and if the line is found, the function
    // returns True and the entire line is returned in strLine.
    // However, if a line starting with strErrorStartText is found first, then the function returns the string
    // in strLine as before, this will contain the offending line, and blnError will be set to true
    bool blnStopSearch = false; // Is a loop stop condition found?
    bool blnFound = false; // Is the line we're looking for found?

    if (*input_file) {
      char ch_FileLine[2048] = "";
      while (!blnStopSearch) {
        blnStopSearch = !(*input_file).getline(ch_FileLine, 2047); // Quit if no line can be read.
        if (!blnStopSearch) {
          // The line was read. Check it.
          *strLine = ch_FileLine;
          if (substr(trim(StringToLower(*strLine)), 0, strStartText.length()) == StringToLower(strStartText)) {
            // The start of this line is what we were looking for.
            blnFound = true;
            blnStopSearch = true;
          } else if ((strErrorStartText != "") &&
                       (substr(trim(StringToLower(*strLine)), 0, strErrorStartText.length()) == StringToLower(strErrorStartText))) {
            // The start of the line matches the text that has been specified to mean that an error has occured.
            // Usually this is the character meaning the marker for the next config file section (past the one
            // we are looking for)
            blnStopSearch = true;
          }
        }
      }
      // We are finished scanning the file for the next line. Return results reflecting the results of the search
      return blnFound; // return True if the line starting with the search string is found.
    }

    else {
      // File was not opened correctly. Return an error condition.
      *strLine = "";
      return false;
    }
    return false;
  }

  // Process_Conf_Line: Read a config line (Line) and break it up into the
  // Identifier (setting name) and an assigned value for that setting.
  // Eg: "Volume  = 100" breaks down into Identifier "Volume" and Value "100"
  // Return true if this was successfully done, otherwise return false if there
  // was a problem - perhaps there is a line error
  void Process_Conf_Line(const string & Line, string &Identifier, string &Value) {
    // Initialize the strings;
    Identifier = Value = "";

    // Quit if the line is empty
    if (Line == "") {
      rr_throw("Config line is empty");
    }

    const char * LinePtr = Line.c_str();

    // Locate the first non-space, non-"=", non ":" character	-	This is the start of the identifier
    while (*LinePtr != 0 && (*LinePtr==' ' || *LinePtr=='=' || *LinePtr==':')) LinePtr++;

    if (*LinePtr == 0) {
      rr_throw("Could not find the first identifier in the config file!");
    }

    // Start copying - find the next space or "=" or ":" character - This is the char after the identifier
    while (*LinePtr !=0 && !(*LinePtr==' ' || *LinePtr=='=' || *LinePtr==':')) {
      char ch = *(LinePtr++);
      Identifier += ch;
    }

    if (*LinePtr == 0) {
      rr_throw("Could not find the char after the identifier!");
    }

    // Locate the first non-space, non-"=", non ":" character	-	This is the start of the value
    while (*LinePtr != 0 && (*LinePtr==' ' || *LinePtr=='=' || *LinePtr==':')) LinePtr++;

    // Copy the remainder of the characters up until the end of the string
    Value = LinePtr;

    // Trim the value (cut off spaces on the start and end)
    Value = trim(Value);
  }

  /***************************************************************************
   *   Daemon-related                                                        *
   ***************************************************************************/

#ifdef __linux__
  void closeall(int fd)
  {
    int fdlimit = sysconf(_SC_OPEN_MAX);

    while (fd < fdlimit)
      close(fd++);
  }
#endif

#ifdef __linux__
  int daemon(int nochdir, int noclose, string strdirectory)
  // fork the current process and remove the original process - call this at the
  // very start of the application. Eg:

  // int result = daemon(0, 0, "/data/radio_retail/prog/mysqlglobalreporter");
  {
      switch (fork())
      {
          case 0:  break;
          case -1: return -1;
          default: _exit(0);          /* exit the original process */
      }

      if (setsid() < 0)               /* shoudn't fail */
        return -1;

      /* dyke out this switch if you want to acquire a control tty in */
      /* the future -- not normally advisable for daemons */

      switch (fork())
      {
          case 0:  break;
          case -1: return -1;
          default: _exit(0);
      }

      if (!nochdir)
        chdir(strdirectory.c_str());

      if (!noclose)
      {
          closeall(0);
          open("/dev/null",O_RDWR);
          dup(0); dup(0);
      }

      return 0;
  }
#endif

  /***************************************************************************
   *   System info                                                           *
   ***************************************************************************/
  string GetHostname() {
    // Use a linux 'ifconfig ' call to ask linux for the machine's IP address
    FILE *fp = NULL;
    char line[130] = "";   /* line of data from unix command* */

    string strHostname = ""; // Intermediate variable for return result.

    // Set a default IP address return value in case we exit with an error
    strHostname = "[Can't find hostname]";

    // Run hostname
    fp = popen("/bin/hostname", "r"); // Run the command

    // Read the 1st (and only) line (if popen doesn't work,
    // fgets doesn't return anything
    fgets(line, sizeof line, fp); // Line 1

    // Close the pipe now that we have the wanted line
    pclose(fp);

    // Now return the line
    if (line!="") {
      strHostname=line;
      // Strip the last character from the line (usually a newline)
      strHostname=substr(strHostname, 0, strHostname.length()-1);
    }
    // Now return the value
    return strHostname;
  }

  string GetIPAddress() {
    // Use a linux 'ifconfig ' call to ask linux for the machine's IP address

    FILE *fp = NULL;
    char line[130] = "";   /* line of data from unix command* */

    // Set a default IP address return value in case we exit with an error
    string strIPAddress = "[Can't find IP]";

    // Run ifconfig
    fp = popen("/sbin/ifconfig", "r"); // Run the command

    // Read the 2 lines (if popen doesn't work,
    // fgets doesn't return anything
    fgets(line, sizeof line, fp); // Line 1
    fgets(line, sizeof line, fp); // Line 2

    // Close the pipe now that we have the wanted line
    pclose(fp);

    // Now decode the line read in
    // - format of the line read...
    //    "inet addr:172.30.166.59  Bcast:172.30.166.255  Mask:255.255.255.0"

    string strLine = line;
    // Is there a string segment "inet addr:" ?
    unsigned intStartPos = strLine.find("inet addr:");

    if (intStartPos == strLine.npos) {
      rr_throw("Error occured!");
    }

    intStartPos+=10;

    // Everything after the colon, up to the first space is the IP address
    unsigned intEndPos = strLine.find(" ", intStartPos);

    if (intStartPos == strLine.npos) {
      rr_throw("Error occured!");
    }

    // Now extract the IP address
    strIPAddress = strLine.substr(intStartPos, intEndPos-intStartPos);

    return strIPAddress;
  }

  long GetUpTime() {
#ifndef __linux__
    undefined_func_throw;
#else
    // Return how many seconds the system has been running for
    struct sysinfo si;
    int result = sysinfo(&si);
    if(result != 0) {
      rr_throw("Abnormal info retrieval... result = " + itostr(result));
    }



    return si.uptime;
#endif
  }

  bool pid_exists(const long lngpid) {
#ifdef __linux__
    return DirExists("/proc/" + itostr(lngpid)+ "/");
#else
   undefined_func_throw;    
#endif      
  }

  unsigned ProcessInstances(const string & strProcessName) {
    // Use a linux 'ps -A' call to ask linux how many times a given pocess is already running
    FILE * fp = NULL;
    char line[130];   /* line of data from unix command*/
    unsigned Instances = 0;

    string ToRun = "ps -A | grep " + strProcessName;
    fp = popen(ToRun.c_str(), "r");   /* Issue the command. */

    /* Read a line			*/
    while (fgets(line, sizeof line, fp))
    {
      // Decode the line
      string strLine = line;
      if (strLine.substr(24) == (strProcessName + "\n")) Instances++;
    }
    pclose(fp);
    return Instances;
  }

  /***************************************************************************
   *   RR-date handling
   ***************************************************************************/

  DateTime RRDateIntToDateTime(const int intRRDate) {
    // Convert an RR date (4 digit number representing days since 1/1/1980) to a DateTime value
    if (intRRDate < 1 || intRRDate > 9998) { // Throw an error if RRDate is not in the allowed range!
      rr_throw("Invalid RR date (outside allowed range 1-9998): " + itostr(intRRDate));
    }    
    return MakeDate(1998, 1, 1) + intRRDate*60*60*24;
  }

  DateTime RRDateToDateTime(const string & RRDate) {
    // Convert an RR date (4 digit number representing days since 1/1/1980) to a DateTime value
    return RRDateIntToDateTime(strtoi(RRDate));
  }

  int GetRRDateInt(const DateTime Date) {
    // Return the number of days since 01/01/1998.
    int intRet = (GetDateTimeDate(Date) - MakeDate(1998, 1, 1))  / (24*60*60);
    if (intRet < 1 || intRet > 9998) {
      rr_throw("Invalid RR date (outside allowed range 1-9998): " + itostr(intRet));      
    }
    return intRet;
  }

  string DateTimeToRRDate(const DateTime Date) {
    // Convert a DateTime value to an RR date string
    int intDays = GetRRDateInt(Date); // This function throws an exception if the RR date is outside of the range 0-9998

    // Now convert to a 4-character string.
    char Buffer[10];
    sprintf(Buffer, "%04d", intDays);

    // Return the string.
    return Buffer;
  }

  /***************************************************************************
   *   Execution, capture output:
   ***************************************************************************/
  int system_capture_out(const string & COMMAND, string & strout) {
    // Do a regular system() call, but return the output out and error in string variables.
    string strout_file= (string)"/tmp/." + PACKAGE + "_output_" + itostr(getpid()) + "_out.txt";
    remove (strout_file.c_str()); // In case it already exists.
    int intret = system(string((string)COMMAND + " &> " + strout_file).c_str());
    strout = ""; // Clear the output
    if (!FileExists(strout_file)) {
      // Output file not found!
      rr_throw("Could not find output file " + strout_file + "!");
    }

    // Now read the file into the return string;
    ifstream infile(strout_file.c_str());
    if (!infile) {
      rr_throw("Could not open " + strout_file + "!");
      return -1;
    }

    // Surely there is a better way to read the entire file into the buffer...:
    int intlinenum = 0;
    string strline = "";
    while (getline(infile, strline)) {
      ++intlinenum;
      if (intlinenum > 1) {
        strout += "\n";
      }
      strout += strline;
    }
    infile.close();

    // Remove the output file:
    remove(strout_file.c_str());

    return intret;
  }

  /***************************************************************************
   *   Communication (net send, e-mail)
   ***************************************************************************/
  
  void net_send(const string & strDest, const string & strMessage) {
    // Send a "NET SEND" popup message to a Windows user.

    // Valid arguments?
    if (trim(strDest) == "") {
      rr_throw("User not specified!");
    }

    if (trim(strMessage) == "") {
      rr_throw("Message not specified!");
    }

    // Does the smbclient binary exist?
    if (!FileExists("/usr/bin/smbclient")) {
      rr_throw("smbclient not installed!");
    }

    // Create and run the NET SEND command:
    string strcmd = "echo \"" + strMessage + "\" | /usr/bin/smbclient -M " + strDest;
    string strcmd_out; // Output of the executed command.
    
    if (system_capture_out(strcmd, strcmd_out) != 0) {
      rr_throw("smbclient: " + strcmd_out);
    }
  }
  
  void send_email(const string & strFrom, const string & strTo, const string & strSubject, const string & strBody) {
    // This function sends an e-mail...

    // Check: Is exim installed?
    if(!FileExists("/usr/sbin/exim")) {
      rr_throw("exim is not installed!");
    }

    // Create a temporary directory to work from:
    temp_dir email_dir("Temporary rr_utils email directory");

    // Create a file to send the email out of:
    string stremail_file = (string)email_dir + "email.txt";
    
    ofstream BufferFile;
    BufferFile.open(stremail_file.c_str(), ios::out);

    // Were we able to open the file?
    if (!BufferFile) {
      rr_throw("Unable to open temporary e-mail buffer file: " + stremail_file);
    }

    // Write the header fields to instruct exim:
    BufferFile << "From: " << strFrom << endl;
    BufferFile << "Subject: " << strSubject << endl;
    // Also an extra newline to mark the beginning of the body
    BufferFile << endl;

    // Append the body of the email:
    BufferFile << strBody << endl;

    // Close the e-mail buffer file:
    BufferFile.close();

    // Build a command to call exim:
    string strcmd = "/usr/sbin/exim -bm " + strTo + " < " + stremail_file;
    string strcmd_out = ""; // The output from exim.

    // Run the command:
    if (system_capture_out(strcmd, strcmd_out) != 0) {
      rr_throw("exim: " + strcmd_out);
    }

    // The temp email file will be deleted by the email_dir object when it destructs...
  }  

  /***************************************************************************
   *   Miscelleneous system calls
   ***************************************************************************/
        
  string get_current_exception_type() {
    // Fetch the type of the current exception. eg "int", "double", "rr_exception".
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

  void RestartLinux() {
    // Restart the PC
    system("reboot");
  }

  void * xmalloc(size_t size) {
    // Call malloc, check the return results, thrown an exception if an error occurs
    // Logic and idea for xmalloc ripped from libc.txt...
    register void *value = malloc (size);
    if (value == 0) {
      rr_throw("virtual memory exhausted");
    }
    return value; // Execution reached this point, return the pointer from malloc.
  }

  /***************************************************************************
   *   String Hash Set
   ***************************************************************************/
  bool KeyInStringHashSet(string_hash_set & Set, const string & str) {
    // Return true if the key is in the string hash set, otherwise false
    string_hash_set::const_iterator IT;
    IT = Set.find(str);
    return IT != Set.end();
  };
}

