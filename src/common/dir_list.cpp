
#ifdef __linux__ // Currently, dir_list support is only available for Linux.

#include "dir_list.h"
#include <sys/stat.h>    // Directory info
#include "my_string.h"
#include "file.h"
#include "exception.h"

#include <vector>
#include "string_splitter.h"

// External data, called by scandir, during the construction of a dir_list object

string strScanPath = ""; // Used for fetching the attribute of a type if d_type is not supported for scandir
vector <string> Ext_Filter; // Used for filtering return files
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

  if (
    // The correct type of file: (symlink/regular/directory, etc):
    ((intd_type & DT_FILTER) != 0) &&
    // Not the "." directory entry:
    (strcmp(entry->d_name, ".") != 0) &&
    // Not the ".." dirctory entry:
    (strcmp(entry->d_name, "..") != 0) &&

    // Ends with one of the extensions specified in dir_list's constructor:
    // - Or if no extensions were specified, allow any:
    ( (Ext_Filter.empty()) ||
      (
        find(Ext_Filter.begin(), Ext_Filter.end(), lcase(get_file_ext(entry->d_name)))
         != Ext_Filter.end()
      )
    )
  ) return true;
  else
    return false;
}

dir_list::dir_list(const string & _strdir, const string & strext_filter, const int intTypeFilt) {
  // Default value for strext_filter is "", intTypeFilt=DT_REG
  strdir = _strdir;

  // Clear the members
  list=NULL; // Under linux this is a pointer...
  intitem_count=0;
  intitem_num=0;

  // The scandir function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strabspathname = relpath_to_abs(strdir, PATH_IS_DIR);

  // Setup the filter variables to be used by the external file selector function file_select()
  // Setup Ext_Filter:
  {
    Ext_Filter.clear();
    string_splitter ext_split(strext_filter);
    while (ext_split) {
      string strext = lcase(ext_split);
      // Remove '.' from the front if found:
      if (!strext.empty() && strext[0] == '.') strext = substr(strext, 1);
      Ext_Filter.push_back(strext);
    }
  }
  // And the other vars:
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
dir_list::operator string() {
  if (intitem_num > intitem_count) my_throw("No more directory entries! (" + strdir + ", " + + ")");

  // More directory entries. Fetch an entry, and go to the next index.
  string strret = list[intitem_num-1]->d_name;
  ++intitem_num;
  return strret;
}

#endif // END: #ifdef __linux__
