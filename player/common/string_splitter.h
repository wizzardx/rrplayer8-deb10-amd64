/***************************************************************************
                          string_splitter.h  -  description
                             -------------------
    begin                : Tue Dec 21 2004
    copyright            : (C) 2004 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

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
  string_splitter(const string & strsearch_in, const string & strdivider=" ", const string & strdone = "", const char chquote = '\0');
  string next();
  operator string () { return next(); } // Functions can use the object as a string to retrieve the next substring.
  int count() { return substrings.size(); } ; // Retrieve substring count
  string operator[](const unsigned int n); // Client code can now refer directly to extracted parts without looping with "next"
private:
  string strRetWhenDone;
  vector <string> substrings; // Broken down during the constructor
  vector <string>::const_iterator it; // Pointer to the next substrings element to bring.
};

#endif
