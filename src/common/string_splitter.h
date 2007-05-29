/// @file
/// String-splitting class, for easy sub-string manipulation.

#ifndef STRING_SPLITTER_H
#define STRING_SPLITTER_H

/**
  *@author David Purdy
  */

#include <string>
#include <vector>

using namespace std;

class string_splitter {
public:
  string_splitter(const string & strsearch_in, const string & strdivider = " ", const char chquote = '\0');
  string next();
  operator string () { return next(); } ///< Functions can use the object as a string to retrieve the next substring.
  int size() const { return substrings.size(); } ; ///< Retrieve substring count
  bool empty() const { return substrings.empty(); } ///< No substrings?
  string operator[](const unsigned int n) const; ///< Client code can refer directly to extracted parts without looping with "next"

  /// Returns true while there are still items to retrieve.
  operator bool() const { return it != substrings.end(); }
private:
  string str; ///< String that was broken down. Kept so we can use it in errors.
  vector <string> substrings; ///< Broken down during the constructor
  vector <string>::const_iterator it; ///< Pointer to the next substrings element to bring.
};

#endif
