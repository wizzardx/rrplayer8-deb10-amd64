
#include <limits.h>
#include <string>
#include <stdlib.h>
#include <stdio.h>

#include "my_string.h"
#include "exception.h"
#include <iostream>

using namespace std;

// Check if a string is a representation of another (basic) type

// Integer
bool isint(const string & str) {
  // Return true if the string is a valid integer...
  try {
    strtoi(str);
    return true;
  }
  catch(...) {
    return false;
  }
}

// Long
bool islong(const string & str) {
  // Return true if the string represents a valid long int.
  try {
    strtol(str);
    return true;
  }
  catch(...) {
    return false;
  }
}

// Unsigned long long
bool isull(const string & str) {
  // Return true if the string represents a valid unsigned long long integer.
  try {
    strtoull(str);
    return true;
  } catch(...) {
    return false;
  }
}

// Double-precision float
bool isdouble(const string & str) {
  try {
    strtod(str);
    return true;
  }
  catch(...) {
    return false;
  }
}

// Boolean
bool isbool(const string & str) {
  // Returns true if the string represents a valid boolean value.
  try {
    strtobool(str);
    return true;
  }
  catch(...) {
    return false;
  }
}

// See also date.h for date & time string conversion & checks

// Convert to other (basic) types

// Integer
int strtoi(const string & str) {
  // Convert a string var to an int. Throw exceptions if there are errors.

  // Make a local copy of the string, and trim it:
  string strnumber = trim(str);

  // Check for emptiness
  if (strnumber == "") {
    my_throw("Cannot convert an empty string to an integer!");
  }

  // Convert to a long:
  char * TailPtr; // *Tailptr points to the character after the last processed
                  // character. If the integer has no problems then *TailPtr
                  // should be '\0'
  long lngNum = std::strtol(strnumber.c_str(), &TailPtr, 10);

  // Check if the entire string was parsed:
  if (*TailPtr != 0) {
    my_throw("Could not convert \"" + strnumber + "\" to an integer");
  }

  // Check if the converted number is in range for an int (we converted to a
  // long because there is no strtoi function in libc:
  if ((lngNum < INT_MIN) || (lngNum > INT_MAX)) {
    my_throw(ltostr(lngNum) + " is outside of the allowed integer range!");
  }

  // Error checks are done. Return the integer:
  return lngNum;
}

// Long
long strtol(const string & str) {
  // Convert a string var to a long. Throw exceptions if there are errors.

  // Make a local copy of the string, and trim it:
  string strnumber = trim(str);

  // Check for emptiness
  if (strnumber == "") {
    my_throw("Cannot convert an empty string to a long integer!");
  }

  // Convert to a long:
  char * TailPtr; // *Tailptr points to the character after the last processed
                  // character. If the integer has no problems then *TailPtr should be '\0'
  long lngNum = std::strtol(strnumber.c_str(), &TailPtr, 10);

  // Check if the entire string was parsed:
  if (*TailPtr != 0) {
    my_throw("Could not convert \"" + strnumber + "\" to a long integer!");
  }

  // Check if the converted number is in the allowed range:
  if ((lngNum < LONG_MIN) || (lngNum > LONG_MAX)) {
    my_throw(strnumber + " is outside of the allowed long integer range!");
  }

  // Error checks are done. Return the long integer:
  return lngNum;
}

// Unsigned long long
unsigned long long int strtoull(const string & str) {
  // Convert a string var to an unsigned long long

  // Make a local copy of the string, and trim it:
  string strnumber = trim(str);

  // Check for emptiness
  if (strnumber == "") {
    my_throw("Cannot convert an empty string to an unsigned long long integer!");
  }

  // Convert to a long:
  char * TailPtr; // *Tailptr points to the character after the last processed
                  // character. If the integer has no problems then *TailPtr should be '\0'

  unsigned long long lngNum = strtoull(strnumber.c_str(), &TailPtr, 10);

  // Check if the entire string was parsed:
  if (*TailPtr != 0) {
    my_throw("Could not convert \"" + strnumber + "\" to an unsigned long long integer!");
  }

  // Check if the converted number is in the allowed range:
  if ((lngNum >= ULONG_LONG_MAX)) {
    my_throw(strnumber + " is outside of the allowed unsigned long long integer range!");
  }

  // Error checks are done. Return the long integer:
  return lngNum;
}

// Double
double strtod(const string & str) {
  // Convert a string var to a double. Throw an exception if there is a problem.

  // Make a local copy of the string, and trim it:
  string strnumber = trim(str);

  // Check for emptiness
  if (strnumber == "") {
    my_throw("Cannot convert an empty string to a double!");
  }

  // Convert to a long:
  char * TailPtr; // *Tailptr points to the character after the last processed
                  // character. If the integer has no problems then *TailPtr should be '\0'
  double dblNum = std::strtod(strnumber.c_str(), &TailPtr);

  // Check if the entire string was parsed:
  if (*TailPtr != 0) {
    my_throw("Could not convert \"" + strnumber + "\" to a double!");
  }

  // Error checks are done. Return the double;
  return dblNum;
}

// Boolean
bool strtobool(const string & str) {
  // Convert a string to a boolean, throw an exception if there is an error.
  string strbool = lcase(trim(str)); // Fetch, trim and conver the arg to lowercase.

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
  if (strbool == "t") return true;
  if (strbool == "f") return false;

  // Cannot determine if strbool represents a "true" or "false" value!
  my_throw("Cannot convert \"" + str + "\" to a boolean value!");
}

// Convert from other (basic) types

// String
string itostr(const int i) {
  // Convert int to char array
  char conv_num[256];
  sprintf(conv_num, "%d", i);
  return conv_num;
}

// Long
string ltostr(const long l) {
  // Convert long to char array
  char conv_num[256];
  sprintf(conv_num, "%ld", l);
  return conv_num;
}

// Unsigned long long
string ulltostr(const unsigned long long l) {
  char conv_num[256];
  sprintf(conv_num, "%lld", l);
  return conv_num;
}

// Double
string dtostr(const double d) {
  char conv_num[256];
  sprintf(conv_num, "%13.6f", d); // eg: "  123456.0000"
  string strret = trim(conv_num); // eg: "123456.0000"

  // Remove all "0" from the end:
  while (right(strret, 1) == "0") {
    strret = left(strret, strret.length() - 1);
  }

  // If the last characer is a decimal point, then remove it also:
  strret = remove_last_char(strret, '.');

  return strret;
}

// Boolean
string booltostr(const bool b) {
  // Return a string representation of a boolean value.
  return (b?"true":"false");
}

// Sub-string manipulation
string left(const string & str, const int left) {
  // Return a string with characters from the left side of the string
  return substr(str, 0, left);
}

string right(const string & str, const int right) {
  // Return a string with characters from the right side of the string
  return substr(str, str.length()-right, str.length());
}

string substr(const string & str, const int from) {
  // My custom version of the substr function - because if you use the
  // string.substr function with an invalid startpos (or len) value it raises
  // an exception. This function returns an empty sting "" instead.
  try {
    return str.substr(from, str.length()); // from is 0-based
  }
  catch(...) { return ""; }
}

string substr(const string & str, const int from, const int len) {
  // My custom version of the substr function - because if you use the string.substr function with an
  // invalid startpos (or len) value it raises an exception. This procedure returns an empty srting ""
  // instead.
  try {
    return str.substr(from, len); // from is 0-based
  }
  catch(...) { return ""; }
}

// Also see string_splitter.h for more advanced string splitting.

// Find & Replace
string replace(const string & search_in, const string & search_for, const string & replace_with, const bool case_sensitive) {
  //
  // * CaseSensitive has a default value of true
  //
  // Search for and replace all occurances of one sub-string with another string.
  string Temp_SearchIn = search_in.c_str(); // C++ = wierd -> this is needed
  string Temp_SearchFor = search_for;

  if (!case_sensitive) {
    Temp_SearchIn = lcase(Temp_SearchIn);
    Temp_SearchFor = lcase(Temp_SearchFor);
  }

  int SearchForLen = search_for.length();

  int ReplaceWithLen = replace_with.length();
  size_t Pos = Temp_SearchIn.find(Temp_SearchFor, 0);

  string strret=search_in;
  while (Pos!=Temp_SearchIn.npos) {
    // Replace in both the SearchIn String and the case-(in)sensitive one.
    strret.replace(Pos, SearchForLen, replace_with);
    Temp_SearchIn.replace(Pos, SearchForLen, replace_with);

    // Find the next postition to replace at
    Pos = Pos + ReplaceWithLen;
    Pos = Temp_SearchIn.find(Temp_SearchFor, Pos);
  }
  return strret;
}

//  - Case sensitive version (easier to remember than using the case_sensitive arg of replace)
string replacei(const string & search_in, const string & search_for, const string & replace_with) {
  // Call Replace with a false CaseSensitive value
  return replace(search_in, search_for, replace_with, false);
}

// Case conversion
string lcase(const string & str) {
  // Convert a string to lower case
  string strRet = str;
  int intLen = strRet.length();

  for(int i=0; i<intLen; i++)
    strRet[i] = tolower(strRet[i]);

  return strRet;
}

string ucase(const string & str) {
  // Convert a string to upper case
  string strRet = str;
  int intLen = strRet.length();

  for(int i=0; i<intLen; i++)
    strRet[i] = toupper(strRet[i]);

  return strRet;
}

// Trimming (outer whitespace removal)
string trim(const string & str) {
  // Remove spaces from the start and the end of a passed string
  string strRetStr = str;

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

// Line-wrapping
string wrap_lines(const string & str, const int min_width, const string & next_line_pad) {
  // Take a possibly very long string in strText and wrap the lines.
  // After each (lngSpaceAfter) characters go onto a new line when a

  // space character is encountered. At the start of the next line
  // insert [strNextLineTabChars] - this is to allow paragraph formatting.

  int intCharsOnLine = 0; // Characters output on the current line in the output string.
  string strReturn = "";  // The final formatted string to return.

  for (long lngPos = 0; lngPos < (int)str.length(); ++lngPos) {
    ++intCharsOnLine;
    string strChar = substr(str, lngPos, 1);
    strReturn += strChar;
    if ((intCharsOnLine >= min_width) && (strChar == " ")) {
      strReturn += "\n" + next_line_pad;
      intCharsOnLine = 0;
    }
  }
  return strReturn;
}

string pad_left(const string & str, const char chpad, const int intfinal_length) {
  // Prepend a character to the left side of a string until the string is long enough.

  // String to prepend:
  string strprepend;
  int intprepend = intfinal_length - str.length();

  // Generate the string:
  for (int i=0; i< intprepend; i++) {
    strprepend += chpad;
  }

  // Return the final string;
  return strprepend + str;
}

string pad_right(const string & str, const char chpad, const int intfinal_length) {
  // Append a character to the right side of a string until the string is long enough:
  string strret = str;
  while ((int)strret.length() < intfinal_length) strret += chpad;
  return strret;
}

// Add dashes to serial keys etc to make them more readable:
string add_dashes(const string & strcode, const int intevery, const char chdash) {
  string strret = "";
  int intpos = 0;
  int intcode_length = strcode.length();
  while (intpos < intcode_length) {
    strret += strcode[intpos];
    if (((intpos + 1) % intevery == 0) && (intpos < intcode_length - 1)) {
      strret += chdash;
    }
    ++intpos;
  }
  return strret;
}

/// Replace a marker in a string (eg: %s or ?) with strings found in the
/// provided string vector.

string format_string_with_vector(const string & str, const vector<string> vec,
                                 const string & replace_marker) {
  unsigned int str_pos = 0;
  unsigned int vec_pos = 0;

  // Replacement marker needs to have at least 1 character:
  if (replace_marker.length() == 0) {
    my_throw("Empty replacement marker string!");
  }

  string ret;
  while (str_pos < str.length()) {
    // Is this character the start of a replacement marker?
    char ch = str[str_pos];
    bool was_replaced = false;
    if (ch == replace_marker[0]) {
      // Yes. Is there a replacement marker here?
      if (substr(str, str_pos, replace_marker.length()) == replace_marker) {
        // Yes, so replace it with a string from the vector
        if (vec_pos >= vec.size()) {
          my_throw("Not enough replacement strings!");
        }
        ret += vec[vec_pos];
        vec_pos += 1;
        str_pos += replace_marker.length() - 1;
        was_replaced = true;
      }
    }
    // If there was no replacement, then append the current character.
    if (!was_replaced) {
      ret += ch;
    }

    // Go to the next character
    ++str_pos;
  }

  // Did we use up all the replacement strings?
  if (vec_pos < vec.size()) {
    int unused = vec.size() - vec_pos;
    if (unused == 1)
      my_throw("1 unused replacement string!");
    else
      my_throw(itostr(unused) + " unused replacement strings!");
  }

  // Return the output string
  return ret;
}

// Quoted strings, eg: ["The dog said \"Woof\""]

bool is_quoted_string(const string & str, const char chquote, bool & blnquote_error) {
  // is_quoted_string: Returns true if the string is a valid, quoted string, and
  // false otherwise. If the input looks like a quoted string (ie, starts with a quote char)
  // but there is an error parsing the string (no closing quote, quote marks without
  // leading "\" chars, etc, then false is returned, and blnquote_error is set to true.
  bool blnret = false;
  blnquote_error = false;

  string strtemp = trim(str);
  if (strtemp.length() >= 2) {
    // The string is long enough to be a quoted string
    if (strtemp[0] == chquote) {
      // The first character is a quote.
      // Check the last character:
      if (strtemp[strtemp.length()-1] != chquote) {
        // String does not end in a quote!
        blnquote_error = true;
      }
      else {
        // Strip the opening and closing quotes:
        strtemp = substr(strtemp, 1, strtemp.length() - 2);
        // Remove any sub-strings that loook like \[quote_char]
        string strslash_quote = "\\";
        strslash_quote += chquote;
        strtemp = replace(strtemp, strslash_quote, "");
        // Now search for any quote characters that remain.
        size_t intquote_pos = strtemp.find(chquote, 0);
        if (intquote_pos != strtemp.npos) {
          // Remaining quote marks were found within the string
          blnquote_error = true;
        }
        else {
          // There were no remaining quote marks.
          // - So this is a completely valid quote string.
          blnret = true;
        }
      }
    }
  }
  return blnret;
}

string quote_string(const string & str, const char chquote) {
  // Quote any string passed to this function, even strings which are already quoted.
  string strret = str; // In case the caller passed the same var in str and strquoted.

  // Replace all occurances of \ with \\ .
  // *HORRIBLE ERROR FOUND!* The line above ends with a \ and so the next line
  // is seen as a continuation! Thanks syntax hilighting! (NOT!)
  strret = replace(strret, "\\", "\\\\");

  // Replace all occurnces of [quote] with \[quote]
  string strquote = "";
  strquote += chquote;
  string strslash_quote = "\\" + strquote;
  strret = replace(strret, strquote, strslash_quote);
  // Add a quote to the beginning and end of the string:
  strret = strquote + strret + strquote;

  // Here we overwrite the output arg.
  return strret;
}

string unquote_string(const string & str, const char chquote) {
  string strret = "";

  // Check if we have a quoted string
  bool blnquote_error = false;
  bool blnquoted = is_quoted_string(str, chquote, blnquote_error);

  // Check for errors:
  if (blnquote_error) my_throw("There is a quoting error with string: " + str);
  if (!blnquoted) my_throw("String is not quoted, cannot unquote it: " + str);

  // A valid quoted string. De-quote it:

  // Remove the starting and ending quotes
  strret = substr(str, 1, str.length() - 2);
  // Parse the string, replace all \[char] sequences with [char]
  // - eg - [\\\"] -> [\"]
  string strprev_ret = strret;
  strret = "";

  const char * buffer = strprev_ret.c_str(); // A char array to search in..
  long lngbuff_len = strprev_ret.length();

  int pos = 0; // Position within the character bufer being searched.

  while (pos < lngbuff_len) {
    char ch = buffer[pos]; // The character at the current pos.
    if (ch == '\\') {
      // A backslash was found.
      // - Skip past the slash, and if possible push the next char into the output
      ch = buffer[pos+1];
      if (ch != 0) {
        ++pos;
        strret += ch;
      }
    }
    else {
      // The current character is just a regular character. Append it to the
      // current part being extracted
      strret += ch;
    }
    ++pos;
  }
  return strret;
}

// Last-character handling

// Remove last char if it matches
string remove_last_char(const string & str, const char c) {
  // Use this function when reading in text files that may have been created
  // under DOS/Windows. Syntax: remove_last_char(string, '\r');
  // DOS/Windows ends each file line with CR/LF (2 charaters - Carriage Return
  // & Line Feed #13#10), while Unix/Linux ends each line with just LF - so
  // when linux reads in lines from a text file created  under Windows, there
  // will be a trailing CR character. Call this function after reading in a
  // string to remove this trailing CR character if it exists
  string strret = str;
  if (strret[strret.length()-1] == c) {
    // Shorten the string by one character
    strret = substr(strret, 0, strret.length()-1);
  }
  return strret;
}

// Add char to end if not found
string ensure_last_char(const string & str, const char c) {
  // Check if a given char is at the end of a string. If it is not there then append it.
  // useful for making sure that paths have a '/' on the end.
  string strret=str;
  if (strret.length() > 0) {
    if (strret[(strret.length())-1] != c) {
      strret += c;
    }
  }
  return strret;
}
