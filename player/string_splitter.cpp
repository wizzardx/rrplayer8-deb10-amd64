/***************************************************************************
                          string_splitter.cpp  -  description
                             -------------------
    version              : v0.02
    begin                : Tue Dec 21 2004
    copyright            : (C) 2004 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "string_splitter.h"
#include "rr_utils.h"

using namespace std;
using namespace rr;

string_splitter::string_splitter(const string & strStringToSearch, const string & strDivider, const string & strDone){
  strLine = strStringToSearch;
  strDividedBy = strDivider;
  strRetWhenDone = strDone;
}

string string_splitter::next() {
  // Return the next substring
  string strret = strRetWhenDone;

  // Return the string the user asked for when there is no more line left;
  if (strLine=="") return strret;

  // If the separator is a space, then ignore whitespace runs:
  unsigned int pos = string::npos;
  if (strDividedBy==" ") {
    // Find the first whitespace character:
    int size=strLine.size();
    bool blnfound = false;
    int i = 0;

    while (i<size && !blnfound) {
      if (isspace(strLine[i]))
        blnfound = true;
      else
        i++;
    }

    // Did we succeed?
    if (blnfound) {
      // Yes: So return the position:
      pos=i;
    }
  }
  else {
    // Not a space. Just do a simple search for the next occurance of the separator:
    pos = strLine.find(strDividedBy);
  }

  if (pos == string::npos) {
    // ~ not found - return the entire string and set the remaining string to 0
    strret = strLine;
    strLine = "";
  }
  else {
    // Found: Return the string up to that point:
    strret = substr(strLine, 0, pos);

    // Set the remaining string to after where the separator was found:
    strLine = substr(strLine, pos + strDividedBy.length(), strLine.length());

    // If the separator is a space, then remove any whitespace from the beginning
    // of the line:
    if (strDividedBy==" ") {
      int size=strLine.size();
      bool blnfound = false;
      int i = 0;

      while (i<size && !blnfound) {
        if (!isspace(strLine[i]))
          blnfound = true;
        else
          i++;
      }

      // Did we find a non-whitespace character?
      if (blnfound) {
        // Yes: So remove the first part of the string up to this point.
        strLine=substr(strLine, i);
      }
      else {   
        // No: The entire remaining string is whitespace. Reset the line now.
        strLine="";
      }
    }
  }

  return strret;
}

