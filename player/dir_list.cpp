/***************************************************************************
                          dir_list.cpp  -  description
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

#ifdef __linux__ // Currently, dir_list support is only available for Linux.

#include "dir_list.h"
#include "rr_utils.h"
#include <sys/stat.h>    // Directory info

using namespace rr;

// External data, called by scandir, during the construction of a dir_list object

string strScanPath = ""; // Used for fetching the attribute of a type if d_type is not supported for scandir
string Ext_Filter = ""; // Used for filtering return files
int DT_FILTER = DT_REG; // The file types to return

int file_select(const struct dirent *entry) {
  // Used for filtering returned files

  // Some systems don't support d_type, so check for this
  int intd_type = entry->d_type;
  if (intd_type == DT_UNKNOWN) {
    struct stat stat_p;
    
    if (stat((strScanPath + entry->d_name).c_str(), &stat_p) != -1) {
      intd_type = stat_p.st_mode / 010000; // Convert from the filetype system used by stat.
    }
  }
  
  if ((strcmp(entry->d_name, ".") != 0) &&
    (strcmp(entry->d_name, "..") != 0) &&
    (Right(StringToLower(entry->d_name), Ext_Filter.length()) == StringToLower(Ext_Filter)) &&
    ((intd_type & DT_FILTER) != 0))
    return (true);
  else
    return (false);
}

dir_list::dir_list(const string & strpathname, const string & strext_filter, const int intTypeFilt){
  // Default value for strext_filter is "", intTypeFilt=DT_REG

  // Clear the members
  list=NULL; // Under linux this is a pointer...
  intitem_count=0;
  intitem_num=0;

  // The scandir function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strabspathname = RelPathToAbs(strpathname, PATH_IS_DIR);

  // Setup the filter variables to be used by the external file selector function file_select()
  Ext_Filter = strext_filter;
  DT_FILTER = intTypeFilt;
  strScanPath = strabspathname;

  // Now run scandir, setup the pointer    
  intitem_count = scandir(strabspathname.c_str(), &list, file_select, alphasort);
  intitem_num = 1; // Point to the first item  
}

dir_list::~dir_list(){
  // Kill the scandir listing ...
  if (list != NULL) {
    // Free all of the individual listing entries.
    for (int i=0; i < intitem_count; i++) {
      if (list[i] != NULL) {
        free(list[i]);
        list[i] = NULL;
      } // end if
    } // end for
    // Now free the array of char dirent pointers..

    free(list);
    list = NULL;
  }
}

  // Fetch the current directory item (and moves to the next). returns "" when there are no more items.
const string dir_list::item() {
  if (intitem_num > intitem_count) {
    // No more directory entries left.
    return "";
  }
  else {
    // More directory entries. Fetch an entry, and go to the next index.    
    string strret = list[intitem_num-1]->d_name;
    ++intitem_num;
    return strret;
  }
}

#endif // END: #ifdef __linux__
