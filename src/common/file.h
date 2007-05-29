/// @file
/// File-system related functions

#ifndef FILE_H
#define FILE_H

#include <string>
#include "my_time.h"

using namespace std;

// FILE HANDLING

// File properties:
bool file_exists(const string & strpath); ///< Does the file exist?
bool file_existsi(const string & strdir, const string & strfname, string & stractual_filename); ///< Does the file exist? strfname is case-insensitive
long file_size(const string & strpath); ///< Return the file size in bytes
datetime file_modified(const string & strPath); ///< Return the file modification date
void chown(const string & strfile, const string & strowner, const string & strgroup); ///< Change a file's ownership.

// Text file scanning:
long count_file_lines(const string & strfilepath); ///< Return the number of lines in a text file
bool find_text_in_file(const string & strtext, const string & strfile_path); ///< Is string strtext anywhere in file strfile_path?

// Writing to a text file:
void append_file_str(const string & strfile, const string & strstring); ///< Append a new line to the end of a text file.

// File copying, moving, deleting, etc:
void cp(const string & strsource, const string & strdest, const string & strdest_permissions = ""); ///< Copy a file from source to destination. Uses an intermediate '.ITF' extention during the copy.
void rm(const string & strfile); ///< Remove a file.
void mv(const string & strsource, const string & strdest); ///< Move a file from source to destination. Uses an intermediate '.ITF' extention during the move.

// Misc file-handling:
string read_symlink(const string & strpath); ///< Returns the location that a symbolic link file points to.

// DIRECTORY
bool dir_exists(const string & strpath); ///< Does a directory exist?
void df(const string & strfile, unsigned long long int & total, unsigned long long int & used, unsigned long long int & available, string & strfilesystem, string & strmounted_on); ///< Calls "df" and retrieves information about a given file or directory's filesystem
void clear_readonly_in_dir(const string & strpath); ///< Reset the "read only" property of all files in a directory.
long count_dir_files(const string &strpath); ///< Return the number of files in a directory.
void mkdir(const string & Path); ///< Create a directory (and any required parents)
void rmdir(const string & strpath); ///< Remove an existing directory (only if empty)

// Current directory, directory of executable:
string getcwd(); ///< Returns the current working directory
void chdir(const string & strpath); ///< Changes the current working directory to a new directory.
string get_exec_path(); ///< Returns the full directory & filename of the binary for the current process. Uses info from /proc/
string get_exec_dir(); ///< Returns just the directory of the binary for the current process. Calls get_exec_path().

// Path string manipulation:
string get_short_filename(const string & strlong_filename); ///< Strips the directory portion of a filename & returns just the file.
string get_file_dir(const string & strlong_filename); ///< Strips the name portion of a filename & returns the directory.
void break_down_file_path(const string & strfilepath, string & strfile_dir, string & strfile_name); ///< Breaks a file path into directory & filename components, and returns these.
string string_to_unix_filename(const string & strfname); ///< Converts a file path into a valid string to use in a command-line. eg "my file.txt" -> "my\ file.txt"
string get_file_ext(const string & strfilepath); ///< Returns the extension of a file, or "" if the file has none.

/// Type used for relpath_to_abs. Paths will always have '/' appended to the end...
enum PATH_TYPE {
  PATH_IS_FILE,
  PATH_IS_DIR
};

string relpath_to_abs(const string & strPath, const PATH_TYPE path_type); ///< Convert a relative path into an absolute path, eg: ~/test.txt -> /root/test.txt

#endif
