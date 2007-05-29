#include <string>
#include "rr_misc.h"
#include "string_splitter.h"
#include "my_string.h"
#include "file.h"
#include "exception.h"
#include <iostream>
#ifdef __linux__
#include <config.h>
#else
  extern const char * VERSION;
  extern const char * PACKAGE;
#endif
#include "system.h"
#include "config_file.h"
#include <fstream>

using namespace std;

// Return true if the filename matches one of the listed prefixes (space-separated string)
// For certain (hardcoded) prefixes (AD CA SP MU FC), chars after the first 2 are not checked.
// eg: SPM, SPC are recognised as SPs. For all other prefixes, the non-alphabetic lead-in
// part muct match. (eg: PC1A does *not* match "PC")
bool rr_media_prefix_matches(const string & strfile, const string & strprefix_list) {
  // Check if the beginning of the file's name matches one of the  listed prefixes.
  // Check args:
  string strprefix = ucase(get_rr_media_prefix(strfile));

  // Does it match one of the listed prefixes?
  {
    string_splitter prefix_list_split(strprefix_list);
    // Compare:
    bool blnfound = false;
    while (prefix_list_split && !blnfound) {
      string strprefix_search = ucase(prefix_list_split);
      // Do we only match the beginning of the file prefix?
      if (strprefix_search == "AD" ||
          strprefix_search == "CA" ||
          strprefix_search == "SP" ||
          strprefix_search == "MU" ||
          strprefix_search == "FC") {
        // Yes. eg: "SPC" and "SPM" match "SP"
        blnfound = left(strprefix, strprefix_search.length()) == strprefix_search;
      }
      else {
        // No. eg: PC1A does not match "PC". An exact match is required.
        blnfound = strprefix == strprefix_search;
      }
    }

    // Return whether we found a match:
    return blnfound;
  }
}

// Convert the filename of an international prerec filename to the South African layout:
// eg: PL1A000001.MP3 ->
//     prl0000001.mp3
// The 2nd arg is set to PC1A, PL1A, etc.
string convert_international_prerec_filename_to_sa(const string & strfile) {
  // PRABCD... -> PRL and PLABCD... -> PRL, etc
  string strnew_file = lcase(get_short_filename(strfile));
  string strleft2 = left(strnew_file, 2);
  // Files starting with pc, pl, ps and pv need to be renamed:
  if (strleft2 == "pc" || strleft2 == "pl" || strleft2 == "ps" || strleft2 == "pv") {
    // Get the new prefix:
    string strnew_prefix = strleft2; // Only pl and ps are different.
    if (strleft2 == "pl") strnew_prefix = "prl";
    else if (strleft2 == "ps") strnew_prefix = "prs";

    // Drop the old prefix from the file:
    string strold_rr_prefix = get_rr_media_prefix(strfile);
    strnew_file = substr(strnew_file, strold_rr_prefix.length());

    // Prepend 0's as necessary...
    while (strnew_file.length() + strnew_prefix.length() < 14) {
      strnew_file = "0" + strnew_file;
    }

    // Now append the new prefix:
    strnew_file = strnew_prefix + strnew_file;
  }
  return strnew_file;
}

// Extract the prefix of an rr media filename. This is done by removing the numeric characters from the end:
string get_rr_media_prefix(const string & strfile) {
  // Remove spaces from the outside:
  string strprefix = trim(strfile);

  // Cut out any path if found:
  strprefix = get_short_filename(strprefix);

  // Throw an exception if the extension is not ".mp3" (adjust this later for .ogg, etc)
  if (lcase(right(strprefix, 4)) != ".mp3") my_throw("File does not end with .mp3!");

  // Strip the extension:
  strprefix = left(strprefix, strprefix.length() - 4);

  // Strip the numeric characters from end of the string:
  {
    const char * chprefix = strprefix.c_str();
    int intpos = strprefix.length() - 1;
    while (isdigit(chprefix[intpos]) && intpos > 0) intpos--;
    strprefix = substr(strprefix, 0, intpos + 1);
  }

  // Convert to upper case:
  return strprefix;
}

// Standard RR file logging (stdout, logfile, and month-day-based logfile rotation)
void rr_log_file(const log_info & LI, const string & strlog_file) {
  // Format into a string for text-based logging:
  string strmessage = format_log(LI, strstandard_log_format);

  // Write to clog:
  clog << strmessage << endl;

  // Log to file:
  append_file_str(strlog_file, strmessage);

  // And some basic log rotation:
  rotate_logfile(strlog_file);
}

// File is a cd track file?
bool file_is_cd_track(const string & strfile) {
  // Return true if the filename is ends with .cda or .cdr
  string strext = lcase(get_file_ext(strfile));
  return strext == "cda" || strext == "cdr";
}

// Write lines to the logfile listing that the program is starting, it's version, etc.
void rr_log_prog_starting(const bool blnfancy) {
  // Now build the line to be logged:
  string strintro_line = (string)PACKAGE + " v" + VERSION;

  // If we have a "fancy" (ie, banner-type display), add a space to the front of the message
  // to be logged:
  if (blnfancy) strintro_line = " " + strintro_line;

  string strequals_line = "";
  for (unsigned i=0; i < strintro_line.length() + 1; ++i) strequals_line += '=';
  if (blnfancy) log_line(strequals_line);
  log_line(strintro_line);
  if (blnfancy) log_line(strequals_line);
}

// Data validation:

string check_not_empty(const string & strfield, const string & strvalue) {
  if (strvalue == "") my_throw(strfield + " is not set!");
  return strvalue;
}

int check_not_empty(const string & strfield, const int intvalue) {
  if (intvalue == -1) my_throw(strfield + " is not set!");
  return intvalue;
}

bool isprice(const string & strprice_arg) {
  // Return true if the argument is in a valid price format.
  string strprice = strprice_arg; // A local copy we can modify

  // Remove the first char if it isn't numeric:
  if (!isint(left(strprice, 1))) strprice = substr(strprice, 1);

  // Make sure that the format is correct:
  // - Cents:
  if (!isint(right(strprice, 2))) return false;

  // - Period before the cents:
  if (substr(strprice, strprice.length() - 3, 1) != ".") return false;

  // - Rands:
  if (!isint(substr(strprice, 0, strprice.length() - 3))) return false;

  // All checks passed:
  return true;
}

