
#include "string_splitter.h"
#include "my_string.h"
#include "exception.h"

using namespace std;

string_splitter::string_splitter(const string & strsearch_in, const string & strdivider, const string & strdone, const char chquote){
  // Default value for chquote - '\0'
  // This class Returns sub-strings from a string that is separated
  // by a specific substring. Additionally, if chquote is defined (not 0), then
  // the entire body of quoted text is considered as a single sub-string, and
  // any occurance of the separater char inside the quoted text is ignored.
  // - Occurances of backslash characters cause the following character
  // to be ignored if it is a quote.
  strRetWhenDone = strdone;
  
  // Perform the substring extraction now, into a string vector. This is so we
  // can return the number of substrings found
  
  // Setup variables for a char array search:
  const char * buffer = strsearch_in.c_str(); // A char array to search in..
  long lngbuff_len = strsearch_in.length();
  int intdivider_len = strdivider.length();
  
  bool blnin_string = false; // Set to True when we are within a quoted string
  int pos = 0; // Position within the character bufer being searched.
  string strpart = ""; // An extracted part;
    
  while (pos < lngbuff_len) {
    char ch = buffer[pos]; // The character at the current pos.
    if (ch == '\\') {
      // A backslash was found. 
      // - Push this character into the output, and the next one if possible
      strpart += ch;
      ch = buffer[pos+1];
      if (ch != 0) {   
        ++pos;
        strpart += ch;
      }
    }
    else if (ch == chquote) {
      // Quotes toggle the current "in a quote" status.
      blnin_string = !blnin_string;
      // - Also include the quote character:
      strpart += ch;      
    }
    else {
      // Not a quote or a backspace character.
      // Are we at the start of a divider?
      bool blndivider=false; // Set to true if the current character is part of a divider
      // Are we in a quoted string at the moment? (ignore dividers)
      if (!blnin_string) {
        // Not in a quoted string at the moment.
        // Are we at a divider?
        // - Is our divider a space character?
        if (strdivider == " ") {
          // Space means consider any whitespace character as a divider.
          // Also, ignore runs of whitespace. (eg: "  X  Y  Z  " extracts 
          // to only 3 fields, not 9)
          if (isspace(buffer[pos])) {
            // Current charcter is whitespace.
            blndivider = true; // We found a divider            
            // Did we find a part to extract?            
            if (strpart != "") { // If it is empty, we most likely have a run of whitespace
              // We found something! Add our section to the list:
              substrings.push_back(strpart);
              strpart = "";
            }                      
          }
        }
        else {
          // No, our divider is not a space character.
          // Is there a divider string, starting at the current character?
          if (substr(strsearch_in, pos, intdivider_len) == strdivider) {
            // Yes! So add the section to the list:
            blndivider = true; // We found a divider
            substrings.push_back(strpart); // You also get empty parts...
            strpart = "";
            // Also advance the current position:
            pos += (intdivider_len - 1);
          }
        }
      }

      // If we weren't at the start of the divider, then this is just a regular
      // character, ie part of the section being extracted.
      if (!blndivider) {
        strpart += ch;
      }
    }
   ++pos; // Go to the next character
  }

  // After parsing the entire string, if we have a part left over, then
  // it is the final part extracted
  if (strpart != "") {
    // Are we in a string? (ie, did the current string not close properly?
    if (blnin_string) my_throw("Last extracted part (" + strpart + ") contains an unclosed string!");

    // No problem. Use the last part:
    substrings.push_back(strpart);      
  }

  // Now set up the iterator for returning sub-strings:
  it=substrings.begin();
}

string string_splitter::next() {
  string strret = "";
  if (it != substrings.end()) {
    strret = *it;
    it++;    
  }
  else {
    strret = strRetWhenDone;
  }
  return strret;
}

string string_splitter::operator[](const unsigned int n) {
  // Client code can now refer directly to extracted parts without looping with "next"

  // Check the index:
  if (n >= substrings.size()) my_throw("Index " + itostr(n) + " is too high. I only have " + itostr(substrings.size()) + " substring(s).");

  // Now return the sub-string:
  return substrings[n];
}
