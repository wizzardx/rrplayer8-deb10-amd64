// Basic buffer handling

#include <string.h>
#include "file.h"
#include <sys/stat.h> // fstat
#include <string>
#include "exception.h"
#include "dir_list.h"
#include "my_string.h"
#include "testing.h"
#include <fstream>
#include "system.h"
#include "string_splitter.h"

using namespace std;

// FILE HANDLING

// File properties:
bool file_exists(const string & strpath) {
  // Return true if the file exists, otherwise return false. Directories and character
  // devices are not considered as "files"
  struct stat stat_p;
  bool Ret_Val = false;

  // The stat function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strStatFriendlyPath = strpath;

  if (strStatFriendlyPath.length() > 0)
    if (strStatFriendlyPath[0] == '~')
      strStatFriendlyPath = getenv("HOME") + string(strStatFriendlyPath, 1);

  if (stat(strStatFriendlyPath.c_str(), &stat_p) != -1)
    Ret_Val = S_ISREG(stat_p.st_mode);  // Only return true if a "regular" file

  return Ret_Val;
}



bool file_existsi(const string & strdir, const string & strfname, string & stractual_filename) {
#ifndef __linux__
  undefined_func_throw;
#else
  // Returns true if a file with a matching filename exists in the folder. A file in a folder matches, if
  // it's filename is the same as the filename being searched for. The search is case insensitive.
  stractual_filename = "";
  bool blnFound = false;
  dir_list Dir(strdir);
  string strDirFile = Dir.item();

  // First do this the easy way - does the file exist? (case sensitive)
  if (file_exists(strdir + strfname)) {
    stractual_filename = strfname;
    return true;
  }

  // Not found - do a case-insensitive search
  string strLowerFName = lcase(strfname);

  while (!blnFound && strDirFile != "") {
    string strTemp;

    char strOldDirFile[1024];
    strcpy(strOldDirFile, strDirFile.c_str());

    strTemp = strDirFile;
    strTemp = lcase(strTemp);
    if (strTemp == strLowerFName) {
      blnFound = true;
      stractual_filename = strOldDirFile;
    }
    strDirFile = Dir.item();
  }

  return blnFound;
#endif
}

long file_size(const string & strpath) {
  // Return the filesize if the file exists, otherwise return -1
  struct stat stat_p;
  long Ret_Val = -1;

  // The stat function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strStatFriendlyPath = strpath;

  if (strStatFriendlyPath.length() > 0)
    if (strStatFriendlyPath[0] == '~')
      strStatFriendlyPath = getenv("HOME") + string(strStatFriendlyPath, 1);

  if (stat(strStatFriendlyPath.c_str(), &stat_p) != -1)
    if S_ISREG(stat_p.st_mode)
      Ret_Val = stat_p.st_size;

  return Ret_Val;
}

datetime get_file_date_modified(const string & strPath) {
  // Fetch the file modifed date/time
  struct stat file_stat;

  int fail = stat(strPath.c_str(), &file_stat);

  if(fail) {
    // could not access the file
    my_throw("Could not access file " + strPath);
  }
  else {
    return file_stat.st_mtime;
  }
}

// Text file scanning:
long count_file_lines(const string & strfilepath) {
  // Count File Lines
  ifstream inFile(strfilepath.c_str());

  if (!inFile) my_throw("Could not open file " + strfilepath + "!");

  string strline;
  int intcount=0;
  while (getline(inFile, strline)) ++intcount;
  return intcount;
}

bool find_text_in_file(const string & strtext, const string & strfile_path) {
  string strCommand = string("/bin/grep \"") + strtext + "\" \"" + strfile_path + "\"" + " &> /dev/null";
  strCommand = replace(strCommand, "$", "\\$");
  return system(strCommand.c_str()) == 0;
}

// Writing to a text file:
void append_file_str(const string & strfile, const string & strstring) {
  // Attempt to open a text file in append mode, append a string, and then close the text file.
  ofstream TextFile;
  TextFile.open(strfile.c_str(), ios::out | ios::app);
  if (!TextFile) my_throw("Could not open file " + strfile + " for appending!");
  
  // Logfile opened, write the log
  TextFile << strstring << endl;
  TextFile.close();
}

// File copying, moving, deleting, etc:
void cp(const string & strsource_arg, const string & strdest_arg) {
  // Logs an error and throws an exception if the copy fails.
  // Copy a file from a source to a destination. Use an intermediate ".ITF" filename.
  // If there are any errors then log an error and throw an exception.

  // Fetch the args into local variables:
  string strsource = strsource_arg;
  string strdest   = strdest_arg;

  // Check that the source file exists before copying:
  if (!file_exists(strsource)) {
    my_throw("Copy failed, file not found: " + strsource);
  }

  // If the destination is a directory (or there is a slash on the end),
  // then add the source filename to the end:
  if (dir_exists(strdest) || (right(strdest, 1) == "/")) {
    // Specified destination is an existing directory. Concatenate the base filename
    // from the source file:
    string strsource_dir="";
    string strsource_file="";
    break_down_file_path(strsource, strsource_dir, strsource_file);
    strdest = ensure_last_char(strdest, '/') + strsource_file;
  }

  // Fetch absolute path versions of the args:
  strsource = relpath_to_abs(strsource, PATH_IS_FILE);
  strdest   = relpath_to_abs(strdest, PATH_IS_FILE);

  // Now concatenate ".ITF" for the temporary filename:
  string strtemp_dest = strdest + ".ITF";

  // Generate and run a command to copy to .ITF:
  string strcmd = "cp " + string_to_unix_filename(strsource) + " " + string_to_unix_filename(strtemp_dest);
  string strcmd_out = ""; // Output of the command.

  if (system_capture_out(strcmd, strcmd_out) != 0) {
    // File copy failed!
    // - Remove the .ITF file if it exists:
    if (file_exists(strtemp_dest)) {
      if (remove(strtemp_dest.c_str()) != 0) {
        // Remove failed!
        my_throw("Removal of temporary file " + strtemp_dest + " failed!");
      }
    }
    // - Now log an error and throw an exception:
    my_throw("Copy failed! Reason: " + strcmd_out);
  }

  // File copy succeeded (source -> temp destination). Now attempt to move the temp destination to the final destination.
  if (rename(strtemp_dest.c_str(), strdest.c_str()) != 0) {
    // Rename from filename.ext.ITF to filename.ext failed!
    // - Remove the temporary .ITF file:
    if (remove(strtemp_dest.c_str()) != 0) {
      // Remove failed!
      my_throw("Removal of temporary file " + strtemp_dest + " failed!");
    }
    // - Now log an error and throw an exception:
    my_throw("Could not rename " + strtemp_dest + " to " + strdest + "!");
  }
  // The copy succeeded. There were no exceptions thrown.
}

void rm(const string & strfile) {
  // Removes a file & throws an exception if the function fails.
  if (remove(strfile.c_str()) != 0) {
    my_throw("rm: \"" + strfile + "\": " + strerror(errno));
  }
}

void mv(const string & strsource, const string & strdest_arg) {
  // Logs an error and throws an exception if the move fails.
  // Move a file from a source to a destination. Use an intermediate ".ITF" filename.
  // If there are any errors then log an error and throw an exception.

  // Check that the source file exists before copying:
  if (!file_exists(strsource)) {
    my_throw("Move failed, file not found: " + strsource);
  }

  // Process the "Destination" arg:
  string strdest = strdest_arg;

  // If the destination is a directory (or there is a slash on the end),
  // then add the source filename to the end:
  if (dir_exists(strdest) || (right(strdest, 1) == "/")) {
    // Specified destination is an existing directory. Concatenate the base filename
    // from the source file:
    string strsource_dir="";
    string strsource_file="";
    break_down_file_path(strsource, strsource_dir, strsource_file);
    strdest = ensure_last_char(strdest, '/') + strsource_file;
  }

  // Attempt to move the file to a temporary ".ITF" location:
  string strtemp_dest = strdest + ".ITF";
  string strcmd = "mv " + string_to_unix_filename(strsource) + " " + string_to_unix_filename(strtemp_dest);
  string strcmd_out = ""; // Output of the command.

  if (system_capture_out(strcmd, strcmd_out) != 0) {
    // File copy failed!
    // - Remove the .ITF file if it exists:
    if (file_exists(strtemp_dest)) {
      if (remove(strtemp_dest.c_str()) != 0) {
        // Remove failed!
        my_throw("Removal of temporary file " + strtemp_dest + " failed!");
      }
    }
    // - Now log an error and throw an exception:
    my_throw("Move failed! Reason: " + strcmd_out);
  }

  // File move succeeded (source -> temp destination). Now attempt to move the temp destination to the final destination.
  if (rename(strtemp_dest.c_str(), strdest.c_str()) != 0) {
    // Rename from filename.ext.ITF to filename.ext failed!
    // - Remove the temporary .ITF file:
    if (remove(strtemp_dest.c_str()) != 0) {
      // Remove failed!
      my_throw("Removal of temporary file " + strtemp_dest + " failed!");
    }
    // - Now log an error and throw an exception:
    my_throw("Could not rename " + strtemp_dest + " to " + strdest + "!");
  }
  // The move succeeded. There were no exceptions thrown.
}

// Basic file I/O - save a buffer to a file or read a file into a buffer.

// Save a buffer to a file.
void save_buffer(char * Data, long lngSize, const string & strFileName) {
  FILE *stream;
  bool blnResult = false;

  stream = fopen (strFileName.c_str(), "wb");
  if (stream == NULL) {

    my_throw("Could not open file for writing: " + strFileName);
  }
  else {
    // File is open, so write
    long lngWritten = fwrite(Data, 1, lngSize, stream);

    if (lngWritten == lngSize) {
      blnResult = true;
    }
    else {
      my_throw("There was an error writing this file! -> " +strFileName);
    }
    fclose(stream);
  }
}

// Read a file into a buffer: Caution: Buffer must be freed by the caller.
void load_buffer(const string & strFileName, char * & pData, long & lngSize) {
  bool blnResult = false;
  pData = NULL;

  FILE * stream = fopen(strFileName.c_str(), "rb");
  if (stream == NULL) {
    // Could not open the file
    my_throw("Could not open file! -> " + strFileName);
  }
  else {
    // File was opened.

    // Fetch the file size
    struct stat file_stats;
    if (fstat(fileno (stream), &file_stats) == 0) {
      // File stats retrieved successfully. Create a buffer of this size and
      // populate it with data from the file.
      lngSize = file_stats.st_size;
      pData = (char*)malloc(lngSize+1); // allocate total size + 1 (for a \0)

      // Read data into the buffer
      long lngRead = fread(pData, 1, lngSize, stream);

      // Place a \0 at the end, so that if this is text data, the string
      // will end here (just in case)
      pData[lngSize]=0;
      blnResult = lngRead == lngSize;
    }
    else {
      // File stats not retrieved.
      my_throw("File stats not retrieved!");
    }

    // Now close the file
    fclose(stream);
  }
}

// Misc file-handling:
string read_symlink(const string & strpath) {
#ifndef __linux__
  undefined_func_throw;
#else
  // Read the path that a symbolic link points to.

  // ...code adapted from the example in libc.txt...
  int size = 100;
  string strRet = "";
  bool blnquit=false; // Set to true when the loop must stop.

  do {
    char *buffer=(char *) xmalloc (size);
    int nchars = readlink (strpath.c_str(), buffer, size);
    if (nchars < 0) {
      // readlink had an error
      blnquit=true;
    } // end if
    else if (nchars < size) {
      // Readlink was successful.
      buffer[nchars] = 0; // - Make sure the string ends at the end of the number
      strRet = buffer; // Populate the return value
      blnquit = true; // No need to grow the buffer
    } // end else

    // Prepare the loop for the next iteration - free the buffer and make the next buffer size larger
    free (buffer);
    size *= 2;
  } while (!blnquit); // end while

  // Now return any Symbolic Link details that were read.
  return strRet;
#endif
}

// DIRECTORY
bool dir_exists(const string & strpath) {
#ifndef __linux__
  undefined_func_throw;
#else
  // Return true if the directory exists, otherwise return false.
  DIR *dir_p;

  // The opendir function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strFriendlyPath = relpath_to_abs(strpath, PATH_IS_DIR);

  dir_p = opendir(strFriendlyPath.c_str());
  bool Exists = dir_p!=NULL;
  closedir(dir_p);

  return Exists;
#endif
}

void df(const string & strfile, unsigned long long int & total, unsigned long long int & used, unsigned long long int & available, string & strfilesystem, string & strmounted_on) {
  // Shell a "DF" call, parse the output, and returns the following information
  // about the filesysem the file (or directory) is mounted on:
  //   A) The total space of the filesystem
  //   B) The used space of the filesystem
  //   C) The total available space
  //   D) The filesystem the specified file (or directory) is on.

  // Reset the args:
  total = used = available = 0;
  strfilesystem = strmounted_on = "";

  // Prepare for a DF call:
  string strcmd="/bin/df -P -B 1 \"" + strfile + "\"";
  string strcmd_out="";

  // Run and check the DF call result:
  if (system_capture_out(strcmd, strcmd_out) != 0) {
    my_throw(strcmd_out);
  }

  // No problem with the DF call, so parse the output
  // eg output:
  //   Filesystem            1-blocks      Used Available Use% Mounted on
  //   /dev/hda3            9844887552 5526700032 3818090496  60% /

  string strline;
  {
    string_splitter split(strcmd_out, "\n", "!!");
    split.next(); // Throw away the first line (we want the 2nd line);
    strline=split; // Get the 2nd line
    string strEND=split; // Check that we only got 2 lines

    if (strEND != "!!") {
      my_throw("Input output from df! I expected 2 lines!");
    }
  }

  // Now split the 2nd line into 6 parts:
  string_splitter split (strline, " ", "!!");


  // - Filesystem
  strfilesystem=split;

  // - 1-blocks (size in bytes)
  total=strtoull(split);
  // - Use
  used=strtoull(split);
  // - Available
  available=strtoull(split);

  split.next(); // Ignore the 5th element - this is DF's usage output...

  // - Mounted on
  strmounted_on=split;

  // Check that we found the correct # of fields:
  if ((string)split != "!!") {
    my_throw("An error parsing DF's output! I expected 6 parts!");
  }
}

void clear_readonly_in_dir(const string & strpath) {
  // Clear the readonly attribute of all files and subfolders in a directory
  if (dir_exists(strpath)) { // Prevent "funny" paths from being run with system()
    string cmd = "chmod 777 -R " + strpath;
    system(cmd.c_str());
  }
}

long count_dir_files(const string &strpath) {
#ifndef __linux__
  undefined_func_throw;
#else
  // Count how many files are in a directory and return this result
  int lngNumFiles = 0;
  dir_list Dir(strpath);


  string strFile = Dir.item();
  while (strFile != "") {
    lngNumFiles++;
    strFile = Dir.item();
  }
  return lngNumFiles;
#endif
}

void mkdir(const string & Path) {
  // Throws an exception if the function fails.
  // Create a directory and all it's parents as necessary
  if (trim(Path) == "") {
    my_throw("Argument to mkdir is empty!");
  }

  string strcmd = "mkdir -p " + string_to_unix_filename(Path);
  string strcmd_out = "";
  if (system_capture_out(strcmd, strcmd_out) != 0) {
    my_throw(strcmd_out);
  }
}

void rmdir(const string & strpath) {
  // Throws an exception if the function fails
  if (::rmdir(strpath.c_str()) != 0) {
    my_throw("rmdir: \"" + strpath + "\": " + strerror(errno));
  }
}

// Current directory, directory of executable:
string getcwd() {
  // Return the current working directory as a string object
  char *buffer = ::getcwd(NULL, 0);
  string strret = buffer;
  free(buffer);
  return strret;
}

void chdir(const string & strpath) {

  // Change the current directory.
  if (::chdir(strpath.c_str()) != 0) {
    my_throw("chdir: \"" + strpath + "\": " + strerror(errno));
  }  
}

// Return the directory and filename of the current process. Read from /proc/
string get_exec_path() {
#ifdef __linux__
  // Return the path and executable that was run to start this process. eg /bin/xmms for XMMS.
  return read_symlink("/proc/" + itostr(getpid()) + "/exe");
#else
  undefined_func_throw;
  return "";
#endif
}

string get_exec_dir() { // Just the directory part of GetExecPath
  // An update for RR apps: Usually RR apps are run as binaries under a "binary" subdir, and are
  // pointed to by a symbolic link in the parent directory. In the case of RR software, we are
  // interested in the parent directory, *not* the "binary" repository.
  string strfile, strpath;
  break_down_file_path(get_exec_path(), strpath, strfile);
  strpath = ensure_last_char(strpath, '/');

  // And now remove the "binary" part at the end.
  if (lcase(right(strpath,8)) == "/binary/") {
    strpath = substr(strpath, 0, strpath.length() - 8);
    strpath = ensure_last_char(strpath, '/');
  }
  return strpath;
}

// Path string manipulation:
string get_short_filename(const string & strlong_filename) {
  // Remove all characters up to and including the first slash / , return.
  // Find the last "/" character
  string strShortFileName = strlong_filename;

  unsigned Pos = strlong_filename.rfind("/");
  if (Pos == string::npos)
    // Couldn't find the last slash - return the string unmodified
    return strShortFileName;

  // Return the filename after this point
  strShortFileName = substr(strShortFileName, Pos + 1, strShortFileName.length());
  return strShortFileName;
}

void break_down_file_path(const string & strfilepath, string & strfile_dir, string & strfile_name) {
  // Take the path of a file, and return the directory and filename of the file
  // Find the right-most slash
  unsigned intpos = strfilepath.rfind("/");
  if (intpos ==string::npos) {
    // No slash found. Assume that the entire string is a filename
    strfile_dir = "";
    strfile_name = strfilepath;
  }
  else {
    // A slash was found. Return the string before (and including), and the string after this point,
    // as the directory name and the file name respectively.
    strfile_dir = substr(strfilepath, 0, intpos + 1);
    strfile_name = substr(strfilepath, intpos + 1);
  }
}

string string_to_unix_filename(const string & strfname) {
  // Use this string to replace " " characters in filenames with "\ "
  return replace(strfname, " ", "\\ ");
}

string relpath_to_abs(const string & strPath, const PATH_TYPE path_type) {
#ifndef __linux__
  undefined_func_throw;
#else
  // Converts a relative path to an absolute path.
  // (has no effect on paths which are already absolute)
  string strret = strPath;

  // First replace any starting $ with the the home path.
  if (strret.length() > 0) {
    if (strret[0] == '~') {
      strret = getenv("HOME") + string(strPath, 1);
    }
  }

  // Now use the stdlib library function for path resolution
  char actualpath [PATH_MAX+1];
  memset(actualpath, '\0', sizeof(actualpath));

//  char *ptr = realpath(strPath.c_str(), actualpath);
  realpath(strret.c_str(), actualpath);
  if (actualpath!="") {
    strret = actualpath;
  }

  // Finally, if this absolute path refers to a directory, then make sure the last
  // character is a "/"
  if (path_type == PATH_IS_DIR) {
    strret = ensure_last_char(strPath, '/');
  }

  return strret;
#endif
}

