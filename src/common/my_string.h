/// @file
/// Some custom string handling

#ifndef MY_STRING_H
#define MY_STRING_H

#include <string>

// hash_fun.h moves around, so use conditional compilation logic to find the
// correct location (borrowed from this bug report:
//     http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=461728)

#  if (__GNUC__ == 3 && __GNUC_MINOR__ < 4)
#    include <ext/stl_hash_fun.h>
#  elif (__GNUC__ == 3 && __GNUC_MINOR__ >= 4 || \
         __GNUC__ == 4 && __GNUC_MINOR__ < 3)
#    include <ext/hash_fun.h>
#  else
#    include <backward/hash_fun.h>
#  endif

using namespace std;

// Check if a string is a representation of another (basic) type
bool isint(const string & str);    ///< String represens a valid int?
bool islong(const string & str);   ///< String represents a valid long int?
bool isull(const string & str);    ///< String represents a valid unsigned long long int?
bool isdouble(const string & str); ///< String represents a valid float?
bool isbool(const string & str);   ///< String represents a valid bool?

// See also date.h for date & time string conversion & checks

// Convert to other (basic) types
int                    strtoi(const string & str);    ///< Convert string to int
long                   strtol(const string & str);    ///< Convert string to long int
unsigned long long int strtoull(const string & str);  ///< Convert string to unsigned long long
double                 strtod(const string & str);    ///< Convert string to float
bool                   strtobool(const string & str); ///< Convert string to boolean

// Convert from other (basic) types
string itostr(const int i);      ///< Convert int to string
string ltostr(const long l);     ///< Convert long to string
string ulltostr(const unsigned long long l); /// Convert unsigned long long to string
string dtostr(const double d);  ///< Convert double to string
string booltostr(const bool b); ///< Convert bool to string

// Sub-string manipulation
string left(const string & str, const int left); ///< Return a sub-string from the left side of a string
string right(const string & str, const int right); ///< Return sub-string from the right side of a string
string substr(const string & str, const int from); ///< Return a sub-string of another string. More friendly than string::substr
string substr(const string & str, const int from, const int len); ///< Return a sub-string of another string. More friendly than string::substr

// Also see string_splitter.h for more advanced string splitting.

// Find & Replace
string replace(const string & search_in, const string & search_for, const string & replace_with, const bool case_sensitive=true); ///< Search a string and replace occurences of one sub-string with another

//  - Case sensitive version (easier to remember than using the case_sensitive arg of replace)
string replacei(const string & search_in, const string & search_for, const string & replace_with);  ///< Search a string and replace occurences of one sub-string with another. Case-insensitive version, easier to remember than using the case_sensitive arg of replace())

// Case conversion
string lcase(const string & str); ///< Convert a string to lower case
string ucase(const string & str); ///< Convert a string to upper case

// Trimming (outer whitespace removal)
string trim(const string & str); ///< Remove whitespace from the beginning and end of a string

// String formatting
string wrap_lines(const string & str, const int min_width, const string & next_line_pad);
string pad_left(const string & str, const char chpad, const int intfinal_length);
string pad_right(const string & str, const char chpad, const int intfinal_length);
string add_dashes(const string & strcode, const int intevery=4, const char chdash='-');

// Quoted strings, eg: ["The dog said \"Woof\""]

// is_quoted_string: Returns true if the string is a valid, quoted string, and
// false otherwise. If the input looks like a quoted string (ie, starts with a quote char)
// but there is an error parsing the string (no closing quote, quote marks without
// leading "\" chars, etc, then false is returned, and blnquote_error is set to true.
bool is_quoted_string(const string & str, const char chquote, bool & blnquote_error);
string quote_string(const string & str, const char chquote);
string unquote_string(const string & str, const char chquote);

// Last-character handling
string remove_last_char(const string & str, const char c); ///< Remove last char if it matches
string ensure_last_char(const string & str, const char c); ///< Add char to end if not found

// Declaration to allow us to use strings with non-standard STL templates:
// (hash_set, hash_map, etc). Copied from
// http://forums.devshed.com/c-programming-42/tip-about-stl-hash-map-and-string-55093.html
namespace __gnu_cxx
{
  template<> struct hash< std::string >
  {
    size_t operator()( const std::string& x ) const
    {
      return hash< const char* >()( x.c_str() );
    }
  };
}

#endif
