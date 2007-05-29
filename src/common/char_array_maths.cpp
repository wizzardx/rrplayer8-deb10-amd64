
#include "char_array_maths.h"
#include "exception.h"
#include "my_string.h"

// Constructor
char_array_maths::char_array_maths(unsigned char * buffer, const int intbuff_len) {
  // Reset internal attributes
  Buffer = NULL;
  intBuffLen = 0;
  intReservedStartChars = 0;
  blnstring_char_repeat_allowed = true;

  // Check the arguments...
  if (buffer == NULL) {
    my_throw("Buffer pointer is null.");
  }
  else if (intbuff_len <= 0) {
    my_throw("Buffer must be at least 1 byte long (" + itostr(intbuff_len) + " is invalid)");
  }
  else {
    Buffer = buffer;
    intBuffLen = intbuff_len;
  }
}

// maths operations. A descriptive exception is thrown if any problems are detected.

void char_array_maths::multiply(const long long lngmultiply_by) {
  // Check the multiplier... must be a non-negative value.
  if (lngmultiply_by < 0) {
    // Cannot multiply by negative numbers...
    my_throw("Multiplication by negative numbers not allowed. Multiplier is " + ltostr(lngmultiply_by));
  }

  // No problem with the multiplier...
  // Create a temporary buffer to place the result in:

#ifdef __linux__
  // Allocate a temporary buffer to place the result in.
  unsigned char * result_buff = (unsigned char *) alloca(intBuffLen); // --> Freed automatically at function exit.
#elif defined __WIN32__
  // alloca() is a BSD extension, not available under Windows.
  unsigned char result_buff[intBuffLen]; // --> Freed automatically at function exit.
#endif

  memset(result_buff, CHAR_ARRAY_0_CHAR, intBuffLen); // Clear the buffer.

  // Now create a char_array_maths instance of the destination buffer.
  char_array_maths result_chars(result_buff, intBuffLen);

  // Multiplication proceeds as follows:
  // Procced from the last digit to the first digit.
  // Multiply each digit by the "multiply by" value, and add the result to the same
  // place in the result characters object. The correct places will be updated by the
  // result characters.

  int intmultiply_pos = intReservedStartChars;
  while (intmultiply_pos < (int) intBuffLen) {
    result_chars.add(lngmultiply_by * (Buffer[intmultiply_pos] - CHAR_ARRAY_0_CHAR), intmultiply_pos);
    intmultiply_pos++;
  }

  // Success: Copy the results to the souce
  memcpy(Buffer, result_buff, intBuffLen); // Clear the buffer.
}

void char_array_maths::divide(const long long lngdivide_by, long & lngremainder) {
  lngremainder = 0;

  // Check for division by 0
  if (lngdivide_by == 0) {
    // Division by zero
    my_throw("Division by zero.");
  }
  // Check for division by negative numbers
  else if (lngdivide_by < 0) {
    // Number to divide by is negative
    my_throw((string) "Division by a negative number is not allowed. Number to divide by is " + ltostr(lngdivide_by));
  }
  else {
    // Not division by 0 or negative.

    // Allocate a temporary buffer to place the calculation results of each cycle in.
    #ifdef __linux__
      // Allocate a temporary buffer to place the result in.
      unsigned char * result_buff = (unsigned char *) alloca(intBuffLen); // --> Freed automatically at function exit.
    #elif defined __WIN32__
        // alloca() is a BSD extension, not available under Windows.
        unsigned char result_buff[intBuffLen]; // --> Freed automatically at function exit.
    #endif

    memset(result_buff, CHAR_ARRAY_0_CHAR, intBuffLen); // Clear the buffer.

    // Now create a char_array_maths instance of the destination buffer.
    char_array_maths result_chars(result_buff, intBuffLen);

    // This function's logic is based on the following example of mathematical logic:
    // Assuming base 10, the sum   543 / x  =    (5*100)/7    +  (4*10)/7   +   (3*1)/7
    // - This function performs divisions of the individual base 256 "digits" (chars) and adds
    // the results together.

    // Now start the division.
    long long lngcalc_remainder = 0; // Calculate the remainder into this variable. Returned later if successful.

    int intdivide_pos = intReservedStartChars;; // Do the positional divisons separately... start from the left and proceed right.
    while (intdivide_pos < intBuffLen) {
      // We start a loop which does a division, takes the remainder to the next
      // lower position (* by position place value) and repeats the div/mod until
      // the remainder we're carrying down, becomes 0, or we run out of places.

      int intcarry_pos = intdivide_pos;

      // long long here used, so that maths involving numbers close to long int limits
      // are less likely to have overflow problems.
      long long lngcarry_remainder = Buffer[intdivide_pos] - CHAR_ARRAY_0_CHAR;

      do {
        result_chars.add(lngcarry_remainder / lngdivide_by, intcarry_pos);
        lngcarry_remainder = (lngcarry_remainder % lngdivide_by) * CHAR_ARRAY_BASE;
        ++ intcarry_pos;
      } while ((intcarry_pos < (int) intBuffLen) && (lngcarry_remainder > 0));

      // If at the end of the loop we have a carry_remainder, then add it to our global remainder, and
      // if the remainder is larger than our "divide by" then increment the lowest part of the result chars.
      lngcalc_remainder += (lngcarry_remainder / CHAR_ARRAY_BASE); // Adjust the carry remainder, it was made
                                                                                                                // The size of one place higher.
      result_chars.add(lngcalc_remainder / lngdivide_by); // Default = add to the last char;
      lngcalc_remainder = lngcalc_remainder % lngdivide_by;

      ++intdivide_pos; // Proceed to the next division.
    }

    // Success: Copy the results to the souce, return success and the remainder;
    memcpy(Buffer, result_buff, intBuffLen); // Clear the buffer.
    lngremainder = lngcalc_remainder;
  }
}

void char_array_maths::add(long long lngadd) {
  add(lngadd, intBuffLen - 1); // Add the integer
}

void char_array_maths::add(long long lngadd, int intplace) {
  // Add the longint, starting at a specific place in the char buff...
  // Check that lngadd is valid
  if (lngadd < 0) {
    my_throw((string) "Addition of negative numbers not allowed. Number to add is " + ltostr(lngadd));
  }
  // Check that intplace is valid...
  else if (intplace >= intBuffLen) {
    // Place to start adding, is after the end of the buffer!
    my_throw("Cannot add to buffer position " + itostr(intplace) + ", buffer is only " + itostr(intBuffLen) + " characters long");
  }
  else if (intplace <  intReservedStartChars) {
    // Place is in a reserved area...
    my_throw("Cannot add to buffer position " + itostr(intplace) + " because the first " + itostr(intReservedStartChars) + " character(s) have been reserved");
  }
  else{
    // Place is valid.
    long long lngVal = lngadd;
    int intPos = intplace;

    while ((lngVal > 0) && (intPos >= intReservedStartChars)) {
      // Calculate the sum at the current position
      long long lngSum = Buffer[intPos] - CHAR_ARRAY_0_CHAR + (lngVal % CHAR_ARRAY_BASE);

      // Set the current buffer position, get the value to add to the next pos.
      Buffer[intPos] = (lngSum % CHAR_ARRAY_BASE) + CHAR_ARRAY_0_CHAR;

      lngVal = (lngVal / CHAR_ARRAY_BASE) + (lngSum / CHAR_ARRAY_BASE);

      --intPos; // Go to the next position..
    }

    // Check if the loop exited with an overflow condition...
    if (intPos < intReservedStartChars && lngVal > 0) {
      // An overflow occured
      my_throw("Overflow occured during addition. Use a larger char buffer.");
    }
  }
}

// Is the numeric value of the buffer 0?
bool char_array_maths::iszero() {
  // Return true if all of the buffer chars are equal to 0;
  bool blnResult = false;
  int intpos = intReservedStartChars;;
  while ((intpos < (int) intBuffLen) && (Buffer[intpos] == CHAR_ARRAY_0_CHAR)) {
    ++intpos;
  }
  blnResult = intpos >= (int) intBuffLen; // Search gets this far if no non-zero characters are found.
  return blnResult;
}

// Reset the buffer chars to 0;
void char_array_maths::reset() {
  memset(Buffer + intReservedStartChars, CHAR_ARRAY_0_CHAR, intBuffLen - intReservedStartChars);
}

// Return a pointer to the first non-zero character... If there are no non-zero characters then
// the position of the last character will be returned.
unsigned char * char_array_maths::first_used_char() {
  unsigned char * chResult = NULL;

  // Find the first non-zero character;
  chResult = Buffer;
  int intpos = intReservedStartChars;
  while ((intpos < (int) intBuffLen) && (*chResult == CHAR_ARRAY_0_CHAR)) {
    ++intpos;
    ++chResult;
  }

  // If all the characters were 0, then return the last character.
  if (intpos >= (int) intBuffLen) {
    // Return the last character.
    chResult = Buffer + intBuffLen - 1;
  }
  return chResult;
}

// Return the number of used characters in the buffer.
int char_array_maths::num_used_chars() {
  // Count the zero characters from left to right. When we
  // encounter a non-zero character, the remaining characters
  // are the return result.
  int intpos = intReservedStartChars;

  while ((intpos < (int) intBuffLen) && (Buffer[intpos] == CHAR_ARRAY_0_CHAR)) {
    ++intpos;
  }
  int intResult = intBuffLen - intpos;

  // Always return at least one character as used.
  if (intResult < 1) {
    intResult = 1;
  }

  return intResult;
}

void char_array_maths::set_reserved_start_chars(const int intreserved_start_chars) {
  // Is the reserved chars count in range?
  if ((intreserved_start_chars >= 0) && (intreserved_start_chars <= intBuffLen)) {
    intReservedStartChars = intreserved_start_chars;
  }
  else {
    // max used chars value is not in the allowed range.
    my_throw("Reserved start characters value " + itostr(intreserved_start_chars) + " is not in the allowed range 0 to " + itostr(intBuffLen));
  }
}

// Utility functions.. Use these to store and retrieve unsigned integers using the minimumn
// number of characters possible. Requirement: Fields must be extracted in the
// reverse order of inclusion.
void char_array_maths::include_value(const long lngvalue, const long lngmin, const long lngmax) {
  // Is lngmin <= lngmax?
  if (lngmin > lngmax) {
    // lngmin is not <= lngmax
    my_throw("Invalid range " + ltostr(lngmin) + " to " + ltostr(lngmax) + " (MIN must be <= MAX)");
  }

  // Multiply the char array by the number of possible values for lngvalue.
  multiply((long long) lngmax - lngmin + 1);

  // Check if the number to include is in the required range...
  if ((lngmin <= lngvalue) && (lngvalue <= lngmax)) {
    // Only if it is within the required range do we add it in...
    add(((long long) lngvalue - lngmin));
  }
  else {
    // Number to be included was outside the allowed range...
    my_throw("Value to include (" + ltostr(lngvalue)+ ") was outside of the specified range " + ltostr(lngmin) + " to " + ltostr(lngmax));
  }
}

void char_array_maths::extract_value(long & lngvalue, const long lngmin, const long lngmax) {
  // Fetch the remainder of dividing by all the possible different values for lngValue.
  lngvalue = -1;

  // Is lngmin <= lngmax?
  if (lngmin <= lngmax) {
    long long lngRange = ((long long) lngmax - lngmin + 1);
    // Extract the field:
    long lngRemainder = 0;
    divide(lngRange, lngRemainder);

    // Division was successful
    // Check the result of the division.
    long lngResult = lngRemainder + lngmin;
    if ((lngResult >= lngmin) && (lngResult <= lngmax)) {
      lngvalue = lngRemainder + lngmin;
     }
    else {
      // The result was not within range! Possibly there was some sort of arithmetic overflow
      my_throw("Extracted value was out of range!");
    }
  }
  else {
    // lngmin is not < lngmax
    my_throw("Invalid range " + ltostr(lngmin) + " to " + ltostr(lngmax) + " (MIN must be <= MAX)");
  }
}

// Some functions to make extraction and inclusion easier for different types...
void char_array_maths::extract_value(char & chvalue, const long lngmin, const long lngmax) {
  long lngvalue = chvalue;
  extract_value(lngvalue, lngmin, lngmax);
  chvalue = lngvalue;
}

void char_array_maths::extract_value(int & intvalue, const long lngmin, const long lngmax) {
  long lngvalue = intvalue;
  extract_value(lngvalue, lngmin, lngmax);
  intvalue = lngvalue;
}

// Include and extract strings.
// All characters besides "allowedchars" are rejected. The minimum required
// char array space will be used to represent these chars.
// "blnno_char_repeat" means that characters in the string are not allowed to
// repeat. If the string to be stored was "aaaaa" and the allowed chars are "ab" then the
// string will be represented as "ababa". This is a convention used for making codes
// easier for humans to read.

// Includes a given string...

void char_array_maths::include_string(const string & strString, const string & strAllowedChars) {
  // Check the string's length.
  if (strString.length() == 0) {
    my_throw("Code is empty.");
  }
  else {
    // We built the string from left to right, so we extract the original buffer by going from right to left.
    const char * code_chars = strString.c_str();

    // The numeric radix (numeric base, eg 16 for hex) being worked with is the number of allowed characters.
    // But if character repetition is not allowed, then this radix value is less by one.
    int intRadix = strAllowedChars.length();

    if (!blnstring_char_repeat_allowed) {
      --intRadix;
    }

    // Is "Radix" a value >= 2?
    if (intRadix < 2) {
      my_throw("Invalid Radix (numeric base) " + itostr(intRadix) + ". Check the allowed characters string is \"" + strAllowedChars + "\"");
    }

    int intcode_pos = 0;
    while (intcode_pos < strString.length()) {
      // Determine the place value of the character.
      char chletter = code_chars[intcode_pos];

      int intletter_index = strAllowedChars.find(chletter);
      int intDigit = intletter_index; // The numeric value of this character...

      // Did we find which allowed char it was?
      if (intletter_index < 0) {
        // Did not!
        my_throw((string) "I found an invalid character '" + chletter + "' in the code.");
      }

      // Was the string encoded so as to prevent repeating characters?
      if (!blnstring_char_repeat_allowed) {
        // Is there a character previous to this one?
        if (intcode_pos > 0) {
          char chprev_letter = code_chars[intcode_pos - 1];
          int intprev_letter_index = strAllowedChars.find(chprev_letter);

          // Was the previous character found in the allowed characters string?
          if (intprev_letter_index >= 0) {
            // Is the previous character the same as the current character?
            if (chletter == chprev_letter) {
              // Letter is repeated
              my_throw((string) "A repeated letter '" + chletter + "' was found in the code.");
            }
            else {
              // If the previous character index was less than the current character index, then the current
              // character was incremented.
              if (intprev_letter_index < intletter_index) {
                --intDigit;
              }
            }
          }
          else {
            // The previous letter is not allowed. This is an error.
            my_throw((string) "I found an invalid character '" + chprev_letter + "' in the code.");
          }
        }
      }

      // Now incorporate the digit into the buffer we are restoring...
      multiply(intRadix);
      add(intDigit);

      // Go to the next letter in the string being imported:
      ++intcode_pos;
    }
  }
}

// Extract a string, using the whole buffer, unless the intmax_chars argument is passed.
string char_array_maths::extract_string(const string & strAllowedChars, const int intmax_chars) {
  // We build the string from left to right...

  // The numeric radix (numeric base, eg 16 for hex) being worked with is the number of allowed characters.
  // But if character repetition is not allowed, then this radix value is less by one.
  int intRadix = strAllowedChars.length();
  if (!blnstring_char_repeat_allowed) {
    --intRadix;
  }

  // Is "Radix" a value >= 2?
  if (intRadix < 2) {
    my_throw("Invalid Radix (numeric base) " + itostr(intRadix) + ". Check the allowed characters string is \"" + strAllowedChars + "\"");
  }

  // No problem, continue. Convert the char array into a string using the new radix
  string strCode = "";
  long lngDigit = 0;
  long lngPrevAllowedIndex = LONG_MAX; // Index into the allowed characters array
  int intLetterNum = 1; // The number of the current letter;
  while ((!iszero()) && (intLetterNum <= intmax_chars)) {
    // Divide the char array by the radix and fetch the remainder. The remainder is our digit
    // to return in the code
    divide(intRadix, lngDigit);

    // No errors,  continue
    // Extablish which character to use from the allowed characters..
    char chChar = 0;

    long lngAllowedIndex = lngDigit;

    if (!blnstring_char_repeat_allowed && (lngAllowedIndex >= lngPrevAllowedIndex)) {
      // No repeated characters allowed, and the current digit is equal to or greater than the previous digit
      ++lngAllowedIndex;
    }
    chChar = strAllowedChars[lngAllowedIndex];
    lngPrevAllowedIndex = lngAllowedIndex; // Remember the character pos used...

    // Prepend the character to the string..
    strCode = chChar + strCode;

    // Goto the next extracted letter...
    ++intLetterNum;
  }

  // String inclusion completed. We are successful if there were no errors.
  return strCode;
}

// A function to return a string containing the chars used for a given numeric base:
// eg: base 2 = "01" and base 16 = "01234567890ABCDEF".
string get_base_allowed_chars(const int intbase) {
  const string strall_chars="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (intbase < 2 || intbase > 36) my_throw("Invalid base " + itostr(intbase) + "!");
  return substr(strall_chars, 0, intbase);
}

