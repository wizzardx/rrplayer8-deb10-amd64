
#include "file.h"
#include <sys/stat.h> // fstat

#include "exception.h"
#include "dir_list.h"
#include "my_string.h"
#include <fstream>
#include "system.h"
#include "string_splitter.h"

#ifdef __linux__
  #include <pwd.h>
  #include <grp.h>
#else
  extern const char * EXEC_PATH;
  #include <io.h>
  #include "dirent.h"
  #include "testing.h"
#endif

using namespace std;

// FILE HANDLING

// File properties:
bool file_exists(const string & strpath) {
  // Return true if the file exists, otherwise return false. Directories and character
  // devices are not considered as "files"
  bool Ret_Val = false;

  struct stat stat_p;

  // The stat function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strStatFriendlyPath = relpath_to_abs(strpath, PATH_IS_FILE);

  if (stat(strStatFriendlyPath.c_str(), &stat_p) == 0)
    Ret_Val = S_ISREG(stat_p.st_mode);  // Only return true if a "regular" file

  return Ret_Val;
}

bool file_existsi(const string & strdir, const string & strfname, string & stractual_filename) {
#ifndef __linux__
  undefined_throw;
#else
  // Returns true if a file with a matching filename exists in the folder. A file in a folder matches, if
  // it's filename is the same as the filename being searched for. The search is case insensitive.
  stractual_filename = "";
  bool blnFound = false;

  // First do this the easy way - does the file exist? (case sensitive)
  if (file_exists(strdir + strfname)) {
    stractual_filename = strfname;
    return true;
  }

  // Not found - do a case-insensitive search
  string strLowerFName = lcase(strfname);
  dir_list Dir(strdir);

  while (!blnFound && Dir) {
    string strDirFile = Dir;
    if (lcase(strDirFile) == strLowerFName) {
      blnFound = true;
      stractual_filename = strDirFile;
    }
  }

  return blnFound;
#endif
}

long file_size(const string & strpath) {
  // Return the filesize if the file exists, otherwise return -1
  struct stat stat_p;

  // The stat function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strStatFriendlyPath = relpath_to_abs(strpath, PATH_IS_FILE);

  CHECK_LIBC(stat(strStatFriendlyPath.c_str(), &stat_p), strStatFriendlyPath);

  return stat_p.st_size;
}

datetime file_modified(const string & strPath) {
  // Fetch the file modifed date/time
  struct stat file_stat;

  CHECK_LIBC(stat(strPath.c_str(), &file_stat), strPath);
  return file_stat.st_mtime;
}

#ifdef __linux__
void chown(const string & strfile, const string & strowner, const string & strgroup) {
  // Fetch OWNER:
  uid_t OWNER;
  {
    passwd RESULT_BUFF;
    passwd * RESULT = &RESULT_BUFF;
    char BUFFER[200];

    CHECK_LIBC(getpwnam_r(strowner.c_str(), RESULT, BUFFER, sizeof(BUFFER), &RESULT), strowner);
    OWNER=RESULT_BUFF.pw_uid;
  }

  // Fetch GROUP:
  gid_t GROUP;
  {
    group RESULT_BUFF;
    group * RESULT = &RESULT_BUFF;
    char BUFFER[200];

    CHECK_LIBC(getgrnam_r(strgroup.c_str(), RESULT, BUFFER, sizeof(BUFFER), &RESULT), strgroup);
    GROUP=RESULT_BUFF.gr_gid;
  }

  // Now attempt to set the owner and group for the file:
  CHECK_LIBC(chown(strfile.c_str(), OWNER, GROUP), strfile);
}
#endif // #ifdef __linux__

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
void cp(const string & strsource_arg, const string & strdest_arg, const string & strdest_permissions) {
  // Logs an error and throws an exception if the copy fails.
  // Copy a file from a source to a destination. Use an intermediate ".ITF" filename.
  // Update permissions if strdest_permissions are set (in chmod syntax)
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

  // File copy succeeded (source -> temp destination). Attempt to set permissions if requested:
  if (strdest_permissions != "") {
    string strcmd = "chmod " + strdest_permissions + " " + string_to_unix_filename(strtemp_dest);
    string strcmd_out = ""; // Output of the command.
    if (system_capture_out(strcmd, strcmd_out) != 0) {
      // Chmod failed!
      // - Remove the .ITF file:
      if (remove(strtemp_dest.c_str()) != 0) {
        // Remove failed!
        my_throw("Removal of temporary file " + strtemp_dest + " failed!");
      }
      // - Now log an error and throw an exception:
      my_throw("Permission change failed! Reason: " + strcmd_out);
    }
  }


  // File copy/chmod succeeded. Now attempt to move the temp destination to the final destination
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

// Misc file-handling:
string read_symlink(const string & strpath) {
#ifndef __linux__
  undefined_throw;
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
  // Return true if the directory exists, otherwise return false.
  // The opendir function does not handle paths with ~ in them. Replace ~ with the value in $HOME env var
  string strFriendlyPath = relpath_to_abs(strpath, PATH_IS_DIR);

  DIR *dir_p = opendir(strFriendlyPath.c_str());
  bool blnexists = (dir_p != NULL);
  closedir(dir_p);

  return blnexists;
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
  system_capture_out_throw(strcmd, strcmd_out);

  // No problem with the DF call, so parse the output
  // eg output:
  //   Filesystem            1-blocks      Used Available Use% Mounted on
  //   /dev/hda3            9844887552 5526700032 3818090496  60% /

  string strline;
  {
    string_splitter split(strcmd_out, "\n");
    split.next(); // Throw away the first line (we want the 2nd line);
    strline=(string)split; // Get the 2nd line

    if ((bool)split) my_throw("Input output from df! I expected 2 lines!");
  }

  // Now split the 2nd line into 6 parts:
  string_splitter split (strline, " ");

  // - Filesystem
  strfilesystem=(string)split;

  // - 1-blocks (size in bytes)
  total=strtoull(split);
  // - Use
  used=strtoull(split);
  // - Available
  available=strtoull(split);

  split.next(); // Ignore the 5th element - this is DF's usage output...

  // - Mounted on
  strmounted_on=(string)split;

  // Check that we found the correct # of fields:
  if ((bool)split) my_throw("An error parsing DF's output! I expected 6 parts!");
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
  undefined_throw;
#else
  // Count how many files are in a directory and return this result
  dir_list Dir(strpath);
  return Dir.size();
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
  CHECK_LIBC(::rmdir(strpath.c_str()), "rmdir: " + strpath);
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
  CHECK_LIBC(::chdir(strpath.c_str()), "chdir: " + strpath);
}

// Return the directory and filename of the current process. Read from /proc/
string get_exec_path() {
#ifdef __linux__
  // Return the path and executable that was run to start this process. eg /bin/xmms for XMMS.
  return read_symlink("/proc/" + itostr(getpid()) + "/exe");
#else
  string strexec_path = EXEC_PATH;
  if (strexec_path == "") my_throw("Please set EXEC_PATH in main()");
  return strexec_path;
#endif
}

string get_exec_dir() { // Just the directory part of GetExecPath
  return get_file_dir(get_exec_path());
}

// Path string manipulation:
string get_short_filename(const string & strlong_filename) {
  string strdir, strfile;
  break_down_file_path(strlong_filename, strdir, strfile);
  return strfile;
}

string get_file_dir(const string & strlong_filename) {
  string strdir, strfile;
  break_down_file_path(strlong_filename, strdir, strfile);
  return strdir;
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
  // This function converts file names with unusual characters - "(", "'", " ", etc, into
  // valid filenames.
  string strret = strfname;
  strret = replace(strret, "\\", "\\\\");
  strret = replace(strret, " ", "\\ ");
  strret = replace(strret, "'", "\\'");
  strret = replace(strret, "(", "\\(");
  strret = replace(strret, ")", "\\)");
  strret = replace(strret, "&", "\\&");
  strret = replace(strret, "`", "\\`");
  return strret;
}

// Returns the extension of a file, or "" if the file has none.
string get_file_ext(const string & strfilepath) {
  string strext = ""; // This is what we return

  string strfile = get_short_filename(strfilepath);

  // Search for a "." from the right side of the filename:
  unsigned intpos = strfile.rfind(".");

  // Did we find it?
  if (intpos != strfile.npos) {
    // Yes. extract the extension:
    strext = substr(strfile, intpos + 1);
  }

  // Return the extension (or "" if we didn't find one):
  return strext;
}

string relpath_to_abs(const string & strPath, const PATH_TYPE path_type) {
#ifndef __linux__
  #warning mingw32 does not yet have realpath so this function will not return the correct output
  return strPath;
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

