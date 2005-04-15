/***************************************************************************
                          temp_dir.h  -  description
                             -------------------
    begin                : Thu Sep 25 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifdef __linux__ // temp_dir support is currently only available for Linux

#ifndef TEMP_DIR_H
#define TEMP_DIR_H

/**Maintains a 1-level temporary directory that lives under prog/tmp/PID/dir_name.
  *@author David Purdy
  */

#include <string>

using namespace std;

class temp_dir {
public: 
  temp_dir(const string & strDescr);   // eg : temp_dir("Temp Dir");
  ~temp_dir(); // Delete the temporary directory, and the parent directory if it is empty also.
  operator string () { return strtemp_dir; } // String operator
private:
  void tidy_old_temp_dirs(); // Remove any temp directories created by program instances that are no longer running.
  
  void create_temp_dir(const string & strDescr); // Called by the constructor
  void remove_temp_dir(); // Called by the destructor
  
  string strtemp_base;  // eg: /tmp/player/
  string strtemp_pid;    // eg: /tmp/player/1234/
  string strtemp_dir;     // eg: /tmp/player/1234/moo/
};

#endif
#endif // END: #ifdef __linux__
