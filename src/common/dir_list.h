/// @file
/// A user-friendly class for listing directory contents

#ifdef __linux__ // dir_list support is currently only available for Linux

#ifndef DIR_LIST_H
#define DIR_LIST_H

// An object for retrieving a directory listing. Better than using the Dir function, because
// Dir() maintains static variables, which means that you cannot have two "Dir sessions"
// open at the same time (eg a function in a directory loop indirectly calls another function
// which uses Dir())

#include <string>
#include <dirent.h>

using namespace std;

class dir_list {
public:
  dir_list(const string & strdir, const string & strext_filter = "", const int intTypeFilt=DT_REG);
  ~dir_list();

  int size()   { return intitem_count; } ///< For functions that want to count directory files
  bool empty() { return intitem_count == 0; }; ///< No files found?
  operator string (); ///< Return the current directory entry.
  void movefirst() { intitem_num = 1; } ///< Go back to the first entry in the listing.

  /// Returns true while there are still items left
  operator bool() const { return intitem_num <= intitem_count; }

private:
    string strdir;     ///< The directory we're getting a list of
    dirent **list;     ///< C system array listing the files we asked for
    int intitem_count; ///< How many files in the directory
    int intitem_num;   ///< Where in the file listing array we are
};

#endif

#endif // END: #ifdef __linux__
