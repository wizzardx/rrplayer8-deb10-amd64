/***************************************************************************
                          temp_dir.cpp  -  description
                             -------------------
    version              : v0.06
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

#ifdef __linux__ // temp_dir support is only available for Linux at this time...

#include "temp_dir.h"
#include "rr_utils.h"
#include "logging.h"
#include "dir_list.h"
#include <unistd.h>
#include "check_library_versions.h" // Always last: Check the versions of included libraries.

using namespace rr;

// CONSTRUCTOR
temp_dir::temp_dir(const string & strdirname){
  // Remove temp directories owned by process instances that are no longer running:
  tidy_old_temp_dirs();

  // Now create the temporary directory:
  create_temp_dir(strdirname);
}

// DESTRUCTOR
temp_dir::~temp_dir() {
  try {
    // Delete the temporary directory, parent directories, etc.
    remove_temp_dir();

    // Now tidy up old directories owned by process instances that are no longer running:
    tidy_old_temp_dirs();
  } catch_exceptions;
}

void temp_dir::tidy_old_temp_dirs() {
// Remove any temporary directories created by program instances that are no longer running.  

  // Generate a base mount directory under the execution directory:
  string strbase_dir = (string)"/tmp/" + PACKAGE + "/";

  // Does this directory exist?
  if (!DirExists(strbase_dir)) return; // Quit if the mount base is not found.

  // The directory does exist: Scan all it's immediate subdirectories: (eg: ../progs/schedmon/mnt/1234/)
  // There could be files as well (eg: something copies a file into the /mnt/PROG_NAME/ directory)
  dir_list base_subdirs(strbase_dir, "", DT_DIR | DT_REG);

  string strpid = base_subdirs.item();
  while (strpid != "") {
    string strpid_dir = strbase_dir + strpid; // Removed the + "/" on the end in case it is a file...

    bool blnpid_found = false; // Set to true if a process owning this directory is found.

    // Is the subdirectory numeric?
    if(!IsInt(strpid)) {
      // Subdirectory name is not numeric! Log a warning:
      log_warning("Found non-numeric subdirectory " + strpid_dir);
    }
    else {
      // Directory is numeric. Is there a running process with this PID?
      blnpid_found = pid_exists(strtoi(strpid));
    }

    // If no PID was found for this directory, so attempt to remove the entire directory structure...
    if (!blnpid_found) {
      // Is the directory actually a regular file?
      if (FileExists(strpid_dir)) {
        log_warning(strpid_dir + " is a file! It will be removed");
      }
      
      // Attempt to remove the entire directory structure:
      string strcmd = "rm -fr " + string_to_unix_filename(strpid_dir);
      string strcmd_out = "";
      if (system_capture_out(strcmd, strcmd_out) != 0) {
        log_warning(strcmd_out);
      }
    }

    // Fetch the next PID subdirectory:
    strpid = base_subdirs.item();
  } // END: while (strpid != "") {

  // Are there any subdirectories left under the base mount directory?
  dir_list base_remaining_subdirs(strbase_dir, "", DT_DIR | DT_REG);
  if (base_remaining_subdirs.count() == 0) {
    // No subdirectories remain. Remove the base mount directory:
    rmdir(strbase_dir); // Throws an exception if there is a problem.
  } // END: if (base_remaining_subdirs.count() == 0)
}

void temp_dir::create_temp_dir(const string & strDescr) {
  // Called by the constructor

  // Fetch the "base" temp directory (/tmp/progname/)
  strtemp_base = (string)"/tmp/" + PACKAGE + "/";

  // Does the temp base directory exist?
  if (!DirExists(strtemp_base)) {
    // No, so create it.
    mkdir(strtemp_base); // This will throw an exception if there are errors.
  }

  // Now create the PID-specific sub-directory under the base temporary directory
  strtemp_pid = strtemp_base + itostr(getpid()) + "/";

  // Does the temp base directory exist?
  if (!DirExists(strtemp_pid)) {
    // No, so create it.
    mkdir(strtemp_pid); // This will throw an exception if there are errors.
  }

  // Now create the specific directory name that was requested (this is only allowed if
  // the specific sub-directory does not already exist)
  strtemp_dir = strtemp_pid + StringToLower(replace(strDescr, " ", "_")) + "/";

  if (DirExists(strtemp_dir)) {
    // The temporary directory was already created by this process! Do not create
    // it a second time... (when we are finished with a temp sub-dir, it should be deleted)
    rr_throw("This directory already exists (" + strDescr + ") : " + strtemp_dir);
  }

  // Now attempt to create the directory
  mkdir(strtemp_dir);          // Throws an exception if there are any errors..
}
   
void temp_dir::remove_temp_dir() {
  // Called by the destructor - firstly remove all files within the temporary directory:

  // Quit if any of the paths are not set:
  if (strtemp_base == "" || strtemp_pid == "" || strtemp_dir == "") return;
  
  // Check if the required directories exist:

  if (!DirExists(strtemp_base)) {
    rr_throw("Directory not found: " + strtemp_base);
  }

  if (!DirExists(strtemp_pid)) {
    rr_throw("Directory not found: " + strtemp_pid);
  }

  if (!DirExists(strtemp_dir)) {
    rr_throw("Directory not found: " + strtemp_dir);
  };

  // Remove the entire contents of strtemp_dir:
  string strcmd = "rm -fr " + string_to_unix_filename(strtemp_dir);
  string strcmd_out = "";
  if (system_capture_out(strcmd, strcmd_out) != 0) {
    log_warning(strcmd_out);
  }

  // Also, attempt to remove the PID-specific temp dir and the base dir (these removes will
  // fail if any of of the directories contains anything.
  try {
    rmdir(strtemp_pid);
    rmdir(strtemp_base);    
  }
  catch(...) { } // Do nothing if rmdir fails.

  // Leave the remianing tidy logic to the "tidy_old_temp_dirs" function (Called by the constructor and destructor)
  // - No need to include further logic for checking and removing the PID and BASE directories here..
}

#endif // END: #ifdef __linux__
