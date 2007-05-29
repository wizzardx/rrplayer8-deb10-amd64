/// @file
/// Mathematics & char arrays.
/// Slow but useful class. Example uses:
/// - Converting between numeric bases
/// - Arbitrary-length & base mathematics
/// - Creating & breaking down codes

#ifndef CHAR_ARRAY_MATHS_H
#define CHAR_ARRAY_MATHS_H

#include <string>

using namespace std;

// Some constants which determine the numeric representation within the char arrays
const int CHAR_ARRAY_BASE = 256;     ///< The calculations are in base 256.
const char CHAR_ARRAY_0_CHAR = '\0';  ///< The value 0 is represented by char \0

class char_array_maths {
public:
  // Constructor
  char_array_maths(unsigned char * buffer, const int intbuff_len);

  void multiply(const long long lngmultiply_by);
  void divide(const long long lngdivide_by, long & lngremainder);
  void add(const long long lngadd);
  void add(const long long lngadd, int intplace);   ///< Add value lngadd, starting at a specific place in a char buff
// Subtraction not yet implemented.
//  bool subtract(unsigned int intsubtract);

  /// Is the numeric value of the buffer 0?
  bool iszero();
  /// Reset the buffer chars to 0
  void reset();
  /// Return the position of the first non-zero character. Returns NULL if none are used.
  unsigned char * first_used_char();
  /// Return the number of characters used in the buffer
  int num_used_chars();

  /// Set some characters at the start of the buffer as being reserved, ie they may not be updated or retrieved
  void set_reserved_start_chars(const int intreserved_start_chars);

  /// Fetch the size of the buffer
  int get_size() { return intBuffLen; }

  // Utility functions.. Use these to store and retrieve unsigned integers using the minimumn
  // number of characters possible. Requirement: Fields must be extracted in the
  // reverse order of inclusion.

  // Include and extract longs, integers, chars, etc...
  void include_value(const long lngvalue, const long lngmin, const long lngmax);
  void extract_value(long & lngvalue, const long lngmin, const long lngmax);

  // Some functions to make extraction and inclusion easier for different types...
  void extract_value(char & chvalue, const long lngmin, const long lngmax);
  void extract_value(int & intvalue, const long lngmin, const long lngmax);

  // Include and extract strings.
  // All characters besides "allowedchars" are rejected. The minimum required
  // char array space will be used to represent these chars.
  // "blnno_char_repeat" means that characters in the string are not allowed to
  // repeat. If the string to be stored was "aaaaa" and the allowed chars are "ab" then the
  // string will be represented as "ababa". This is a convention used for making codes
  // easier for humans to read & type in SMS messages.

  /// Includes a given string
  void include_string(const string & strString, const string & strAllowedChars);

  /// Extract a string, using the whole buffer, unless the intmax_chars argument is passed.
  string extract_string(const string & strAllowedChars, const int intmax_chars = INT_MAX);

  /// This function affects include_string() and extract_string().
  void string_char_repeat_allowed(const bool blnallowed) { blnstring_char_repeat_allowed = blnallowed; } ;

private:
  /// Buffer
  unsigned char * Buffer;
  int intBuffLen;

  // Characters that are currently reserved at the start of the buffer:
  // Maximum characters in the buffer that can be updated or retried from:

  /// This effectively reserves characters at the start of the buffer
  int intReservedStartChars;

  /// Is code character repetition allowed in include_string() & extract_string()?
  // See earlier comment...
  bool blnstring_char_repeat_allowed;
};

/// A function to return a string containing the chars used for a given numeric base:
/// eg: base 2 = "01" and base 16 = "01234567890ABCDEF"
string get_base_allowed_chars(const int intbase);

#endif
