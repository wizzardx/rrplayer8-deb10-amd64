/***************************************************************************
                          dir_list.h  -  description
                             -------------------
    version              : v0.03
    begin                : Thu Sep 25 2003
    copyright            : (C) 2003 by David Purdy
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

#ifdef __linux__ // dir_list support is currently only available for Linux


#ifndef DIR_LIST_H
#define DIR_LIST_H
#define DIR_LIST_H_VERSION 3 // Meaning 0.03

/**
  *@author David Purdy
  */

// An object for retrieving a directory listing. Better than using the Dir function, because
// Dir() maintains static variables, which means that you cannot have two "Dir sessions"
// open at the same time (eg a function in a directory loop indirectly calls another function
// which uses Dir())

#include <string>
#include <dirent.h>

using namespace std;

#include "check_library_versions.h" // Always last: Check the versions of included libraries.

class dir_list {
public: 
  dir_list(const string & strpathname, const string & strext_filter = "", const int intTypeFilt=DT_REG);
  ~dir_list();
  // Fetch the current directory item (and moves to the next). returns "" when there are no more items.
  const string item();
  const int count() { return intitem_count; } // For functions that want to count directory files
  operator string () { return item(); } // Functions can use the object as a string to retrieve the next dir item.
private:
    dirent **list;   // C system array listing the files we asked for
    int intitem_count;              // How many files in the directory
    int intitem_num;               // Where in the file listing array we are
};

#endif

#endif // END: #ifdef __linux__
