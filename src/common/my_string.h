// Some custom string handling

#ifndef MY_STRING_H
#define MY_STRING_H

#include <string>

using namespace std;

// Check if a string is a representation of another (basic) type
bool isint(const string & str);    // Integer
bool islong(const string & str);   // Long
bool isull(const string & str);    // Unsigned long long
bool isdouble(const string & str); // Double-precision float
bool isbool(const string & str);  // Boolean

// See also date.h for date & time string conversion & checks

// Convert to other (basic) types
int                    strtoi(const string & str);    // Integer
long                   strtol(const string & str);    // Long
unsigned long long int strtoull(const string & str);  // Unsigned long long
double                 strtod(const string & str);    // Double
bool                   strtobool(const string & str); // Boolean

// Convert from other (basic) types
string itostr(const int i);      // Integer
string ltostr(const long l);     // Long
string ulltostr(const unsigned long long l);   // Unsigned long long
string dtostr(const double d);   // Double
string booltostr(const bool b);  // Boolean

// Sub-string manipulation
string left(const string & str, const int left);
string right(const string & str, const int right);
string substr(const string & str, const int from);
string substr(const string & str, const int from, const int len);

// Also see string_splitter.h for more advanced string splitting.

// Find & Replace
string replace(const string & search_in, const string & search_for, const string & replace_with, const bool case_sensitive=true);

//  - Case sensitive version (easier to remember than using the case_sensitive arg of replace)
string replacei(const string & search_in, const string & search_for, const string & replace_with);

// Case conversion
string lcase(const string & str);
string ucase(const string & str);

// Trimming (outer whitespace removal)
string trim(const string & str);

// Line-wrapping
string wrap_lines(const string & str, const int min_width, const string & next_line_pad);

// Quoted strings, eg: ["The dog said \"Woof\""]

// is_quoted_string: Returns true if the string is a valid, quoted string, and
// false otherwise. If the input looks like a quoted string (ie, starts with a quote char)
// but there is an error parsing the string (no closing quote, quote marks without
// leading "\" chars, etc, then false is returned, and blnquote_error is set to true.
bool is_quoted_string(const string & str, const char chquote, bool & blnquote_error);
string quote_string(const string & str, const char chquote);
string unquote_string(const string & str, const char chquote);

// Last-character handling
string remove_last_char(const string & str, const char c); // Remove last char if it matches
string ensure_last_char(const string & str, const char c); // Add char to end if not found

#endif
