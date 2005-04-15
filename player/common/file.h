
#ifndef FILE_H
#define FILE_H

#include <string>
#include "my_time.h"

using namespace std;

// FILE HANDLING

// File properties:
bool file_exists(const string & strpath);
bool file_existsi(const string & strdir, const string & strfname, string & stractual_filename);
long file_size(const string & strpath);
datetime get_file_date_modified(const string & strPath);

// Text file scanning:
long count_file_lines(const string & strfilepath);
bool find_text_in_file(const string & strtext, const string & strfile_path);

// Writing to a text file:
void append_file_str(const string & strfile, const string & strstring); // Append text to a file

// File copying, moving, deleting, etc:
void cp(const string & strsource, const string & strdest); // Logs an error and throws an exception if the copy fails.
void rm(const string & strfile); // Removes a file & throws an exception if the function fails.
void mv(const string & strsource, const string & strdest); // Logs an error and throws an exception if the move fails.

// Memory buffer <--> File
// Save a buffer to a file.
void save_buffer(char * data, long lngsize, const string & strfile);

// Read a file into a buffer: Caution: Buffer must be freed by the caller.
void load_buffer(const string & strfilename, char * & pdata, long & lngsize);

// Misc file-handling:
string read_symlink(const string & strpath);

// DIRECTORY
bool dir_exists(const string & strpath);
void df(const string & strfile, unsigned long long int & total, unsigned long long int & used, unsigned long long int & available, string & strfilesystem, string & strmounted_on);
void clear_readonly_in_dir(const string & strpath);
long count_dir_files(const string &strpath);
void mkdir(const string & Path); // Throws an exception if the function fails.
void rmdir(const string & strpath); // Throws an exception if the function fails

// Current directory, directory of executable:
string getcwd();
void chdir(const string & strpath);
string get_exec_path(); // Return the directory and filename of the current process. Read from /proc/
string get_exec_dir(); // Just the directory part of GetExecPath

// Path string manipulation:
string get_short_filename(const string & strlong_filename);
void break_down_file_path(const string & strfilepath, string & strfile_dir, string & strfile_name);
string string_to_unix_filename(const string & strfname);

enum PATH_TYPE { // Used for RelPathToAbs, paths will always have '/' appended to the end...
  PATH_IS_FILE,

  PATH_IS_DIR
};

string relpath_to_abs(const string & strPath, const PATH_TYPE path_type);

#endif
