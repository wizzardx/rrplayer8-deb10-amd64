/***************************************************************************
                          string_splitter.h  -  description
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

#ifndef STRING_SPLITTER_H
#define STRING_SPLITTER_H
#define STRING_SPLITTER_H_VERSION 2 // Meaning v0.02

/**
  *@author David Purdy
  */

#include <string>
#include "check_library_versions.h" // Always last: Check the versions of included libraries.

using namespace std;

class string_splitter {
public:
  string_splitter(const string & strStringToSearch, const string & strDivider, const string & strDone="");
  string next();
  operator string () { return next(); } // Functions can use the object as a string to retrieve the next substring.
private:
  string strLine;
  string strDividedBy;
  string strRetWhenDone;  
};

#endif
