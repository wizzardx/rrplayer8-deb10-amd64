
#ifdef __linux__ // windows_mount support is only available for Linux...

#include "windows_mount.h"
#include "rr_security.h" // For decrypting encrypted login passwords.
#include "exception.h"
#include "unistd.h" // for rmdir
#include "stdio.h" // for FILE
#include "dir_list.h" // for dir_list
#include <config.h>
#include "file.h"
#include "system.h"

// Constructor

windows_mount::windows_mount(const string & strwindows_path, const string & strlogin_user, const string & strencrypted_login_password, const string & strmnt_descr, const unsigned long long lngmin_free_space) {
  // Reset object attributes:
  clear();

  // Remove mount directories used by program instances that are no longer running:
  tidy_old_mounts();

  // Call another procedure to do the mounting (it's a pain to debug constructors, breakpoints don't work under kdevelop!)
  do_mount(strwindows_path, strlogin_user, strencrypted_login_password, strmnt_descr, lngmin_free_space);
}

// Destructor.
windows_mount::~windows_mount(){
  // If this object created the mount (as opposed to borrowing the share from a previous instance) then
  // unmount it now:
  if (blncreated_mount) {
    do_unmount();
  }

  // Now tidy up any mount directories used by program instances that are no longer running:
  tidy_old_mounts();
}

// If the construction succeeded, return the directory that the user wanted (probably a sub-directory on a
// networked machine.
string windows_mount::local_path() const {
  return strfull_path;
}

// Retrieve the original Windows path that was mounted
string windows_mount::remote_path() const {
  return strwindows_path;
}

void windows_mount::clear() {
  // Reset object attributes
  strmnt_descr    = "";
  strwindows_path = "";
  strmount_base   = "";
  strmount_pid    = "";
  strmount_dir    = "";
  strfull_path    = "";
  blncreated_mount = false;
}

// do_mount - called by the constructor, because kdevelop can't catch breakpoints in constructors!
void windows_mount::do_mount(const string & strwindows_path_arg, const string & strlogin_user, const string & strencrypted_login_password, const string & strmnt_descr_arg, const unsigned long long lngmin_free_space) {
  // Check the arguments

  // mount description
  if (strmnt_descr_arg == "") {
    // Windows path not set!
    my_throw("mount description argument is empty (" + strwindows_path_arg + ")");
  }
  strmnt_descr = strmnt_descr_arg;

  // Windows path
  if (strwindows_path_arg == "") {
    // Windows path not set!
    my_throw("Windows path argument is empty (" + strmnt_descr + ")");
  }

  if (strlogin_user == "") {
    // Username for samba mount not set!
    my_throw("samba user argument is empty (" + strmnt_descr + ")");
  }

  // login_password
  if (strencrypted_login_password == "") {
    // Windows path not set!
    my_throw("samba password argument is empty (" + strmnt_descr +")");
  }

  // Is smbmount installed?
  if (!file_exists("/usr/bin/smbmount")) {
    // smbmount is not installed!
    my_throw("smbmount not found, please install Debian package \"smbfs\"");
  }

  // Copy the windows path so that "remote_path()" can retrieve it.
  // - Also replace "\\" with "/"
  strwindows_path = replace(strwindows_path_arg, "\\", "/");

  // windows_mount classes now also allow "dummy" usage, ie, just pointing to an
  // existing local path:
  if (left(strwindows_path, 2) != "//") {
    // Run in "dummy" mode:
    if (!dir_exists(strwindows_path)) my_throw("Directory not found: " + strwindows_path);
    strfull_path = ensure_last_char(strwindows_path, '/');
  }
  else {
    // Run in a regular mode, ie make a samba mount or reuse an existing one:

    // Generate the service name to be mounted, as well as the path to append after the
    // service name to get the path on the server we're interested in
    string strservice_name = "";
    string strservice_subdir = "";

    // Are the first 2 characters "//" ?
    if (substr(strwindows_path, 0, 2) != "//") {
      my_throw("Windows path '" + strwindows_path_arg + "' (" + strmnt_descr + ") must start with '\\\\'");
    }

    // Is there another "/" in the path after the initial "//" ?
    int intslash_3_pos = strwindows_path.find("/", 2);
    int intslash_4_pos = strwindows_path.find("/", intslash_3_pos + 1);

    if (intslash_3_pos == strwindows_path.npos) {
      my_throw("Windows path '" + strwindows_path_arg + "' (" + strmnt_descr + ") must have another '\\' character after the initial '\\\\'");
    }

    //There must be more characters after the 3rd "\"
    if ((signed) strwindows_path.length() <= (intslash_3_pos + 1)) {
      my_throw("Windows path '" + strwindows_path_arg + "' (" + strmnt_descr + ") must have more characters after the 3rd '\\' character");
    }

    // Split up the Windows path into the service name and the sub-directory
    // Do we have a 4th slash character?
    if (intslash_4_pos == strwindows_path.npos) {
      // No 4th slash. ie the entire string is the service name, and we have no
      // service subdirectory
      strservice_name = strwindows_path;
      strservice_subdir = "";
    }
    else {
      // We have a 4th slash, the service name and subdirectory are
      // separated by this character.
      strservice_name = substr(strwindows_path, 0, intslash_4_pos);
      strservice_subdir = substr(strwindows_path, intslash_4_pos + 1);
    }

    // Check that we extracted a service name:
    if (strservice_name == "") {
      my_throw("Error extracting service name from windows path \"" + strwindows_path_arg + "\" (" + strmnt_descr + ")");
    }

    // Make sure that the service subdirectory has a slash on the end...
    // (assuming there is a service subdirectory)
    if (strservice_subdir != "") {
      strservice_subdir = ensure_last_char(strservice_subdir, '/');
    }

    // Generate a base mount directory, based on the execution dir:
    strmount_base = (string)"/mnt/" + PACKAGE + "/";

    // Does this directory already exist?
    if (!dir_exists(strmount_base)) {
      // No: Attempt to create it:
      mkdir(strmount_base); // Throws an exception + logs an error on failure.
    }

    // Now generate a PID-specific subdirectory:
    strmount_pid = strmount_base + itostr(getpid()) + "/";  // eg: /mnt/schedmon/1234/

    // Does this directory already exist?
    if (!dir_exists(strmount_pid)) {
      // No: Attempt to create it:
      mkdir(strmount_pid); // Logs an error and throws an exception on failure
    }

    // Generate a share-specific subdirectory for this PID (eg: ...progs/schedmon/mnt/1234/mail_data_whouse/)
    strmount_dir=strmount_pid + lcase(replace(substr(strservice_name, 2), "/", "_") + "/");

    bool blnreuse_mount = false; // Set to true if we find that the service we want to mount at this directory,
                                 // has already been mounted there.

    // Does the mount directory already exist?
    if (dir_exists(strmount_dir)) {
      // Yes: Fetch any SAMBA service that is already mounted at this path:
      string strmounted_service = get_first_mounted_samba_service(strmount_dir);

      // Is there a service mounted there?
      if (strmounted_service == "") {
        // No: Attempt to remove the directory
        rmdir(strmount_dir); // Throws an exception if there is an error.
      }
      else {
        // Yes: Is it the service we were asked to mount there?
        if (lcase(strmounted_service) == lcase(strservice_name)) {
          // Yes: The share was already created. We will attempt to reuse it.
          blnreuse_mount = true;
        }
        else {
          // No: An unknown service is already mounted there!
          my_throw("An unknown service \"" + strmounted_service + "\" is already mounted at " + strmount_dir);
        }
      }
    }

    // If the mount directory doesn't exist at this point, attempt to create it:
    if (!dir_exists(strmount_dir)) {
      // No: Attempt to create it:
      mkdir(strmount_dir); // Logs an error and throws an exception on failure.
    }

    // Are we reusing an existing mount?
    if (!blnreuse_mount) {
      // No: We need to create the mount:
      // Create the linux command.
      string strdecrypted_password=decrypt_string(strencrypted_login_password, get_rr_encrypt_key(), 2);

      string strCMD = "smbmount \"" + strservice_name + "\" \"" + strmount_dir + "\" -o username=" + strlogin_user + ",password=" + strdecrypted_password + " 2>&1";
      string strsmbmount_out = "";

      blncreated_mount = true; // Set here so the destructor will attempt to remove the mount.
      int intsmbmount_ret = system_capture_out(strCMD, strsmbmount_out);

      // Replace all occurances of newline...
      strsmbmount_out = replace(strsmbmount_out, "\n", ". ");

      // Check the smbmount output and return lines:
      if (intsmbmount_ret != 0 || strsmbmount_out != "") {
        my_throw("Could not mount \"" + strservice_name + "\" (" + strmnt_descr + "). " + strsmbmount_out);
      }
    }

    // The share is now mounted. Does the subdirectory the user is interested in, exist?
    if (!dir_exists(strmount_dir + strservice_subdir)) {
      my_throw("Directory does not exist: " + strwindows_path + "(" + strmnt_descr + ")");
      // The destructor will unmount and remove the directory...
    }

    // The directory found under the mount:
    strfull_path = strmount_dir + strservice_subdir;
  }

  // Check if there is enough free space under the share we just mounted:
  unsigned long long int lngdummy; // For unwanted return values
  string strdummy; // For unwanted return values
  unsigned long long int lngavailable; // Space available under the share.
  df(strfull_path, lngdummy, lngdummy, lngavailable, strdummy, strdummy);

  if (lngavailable < lngmin_free_space) {
    long lngMB = lngavailable/(1024*1024);
    my_throw("Free space is running low! " + strwindows_path + " has only " + ltostr(lngMB) + " MB remaining!");
  }
}

void windows_mount::do_unmount() {
  // Called by the destructor to umount a mounted share (only if the object actually created the mount,
  // ie not borrowing the share from another object instance)

  // Did this object actually create the mount that was returned?
  if (blncreated_mount) {
    // Yes: So attempt to unmount it:
    string strCMD = "smbumount \"" + strmount_dir + "\"";
    string strsmbmount_out = "";
    int intsmbmount_ret = system_capture_out(strCMD, strsmbmount_out);
    // Replace all occurances of newline in the output...
    strsmbmount_out=replace(strsmbmount_out, "\n", ". ");

    // Check the smbmount output and return lines:
    if (intsmbmount_ret != 0 || strsmbmount_out != "") {
      // Unmount failed!
      my_throw("Could not unmount directory " + strmount_dir + " (" + strmnt_descr + "). " + strsmbmount_out);
    }
    else {
      // Unmount succeeded. Remove the mount directory:
      rmdir(strmount_dir); // Throws an exception if there is an error

      // Now some miscelleneous directory cleanups
      // (These will only succeed if this is the last windows_mount object that was active for the app)
      // - We don't need any major checking here, the "tidy_old_mounts" function does that well enough already
      try {
        rmdir(strmount_pid);        // eg: .../progs/shedmon/mnt/1234/ - This will fail if there are any other mount directories.
        rmdir(strmount_base);     // eg: .../progs/shedmon/mnt/ - This will fail if there are any other PID directories.
      } catch(...) {} // Do nothing if either of the rmdir calls throws an exception...
    } // END: if (intsmbmount_ret != 0 || strsmbmount_out != "")
  } // END: if (blncreated_mount)
}

void windows_mount::tidy_old_mounts() {
  // Remove any mount directories created by program instances that are no longer running.

  // Generate a base mount directory under the execution directory:
  string strbase_dir = (string) "/mnt/" + PACKAGE + "/"; // eg: /mnt/schedmon/

  // Does this directory exist?
  if (!dir_exists(strbase_dir)) return; // Quit if the mount base is not found.

  // The directory does exist: Scan all it's immediate subdirectories: (eg: /mnt/schedmon/1234/)
  dir_list base_subdirs(strbase_dir, "", DT_DIR);
  while (base_subdirs) {
    string strpid = base_subdirs;
    string strpid_dir = strbase_dir + strpid + "/";

    bool blnpid_found = false; // Set to true if a process owning this directory is found.

    // Is the subdirectory numeric?
    if(!isint(strpid)) {
      // Subdirectory name is not numeric! Log a warning:
      log_warning("Found non-numeric subdirectory " + strpid_dir);
    }
    else {
      // Directory is numeric. Is there a running process with this PID?
      blnpid_found = pid_exists(strtoi(strpid));
    }

    // If no PID was found for this directory, attempt to go into it, unmount subdirectories, remove them, etc:
    if (!blnpid_found) {
      // No process with this ID is running. Check for subdirectories: (eg: ..progs/schedmon/mnt/1234/mail_data_whouse/)
      dir_list pid_subdirs(strpid_dir, "", DT_DIR);
      while (pid_subdirs) {
        string strmnt = pid_subdirs;
        string strmnt_dir = strpid_dir + strmnt + "/";
        // Check if a samba resource is mounted here:

        // It's possible for the same directory to be smbmount'ed several times. So keep unmounting the same dir until
        // there is an error or there are no more samba resources mounted here.
        bool blncheck_mount_again = false; // Set to true within the loop if the umount should go again.
        do {
          blncheck_mount_again = false; // Reset the loop check variable.

          if (get_first_mounted_samba_service(strmnt_dir) != "") {
            // There is a samba resource mounted here. Attempt to unmount it:
            string strCMD = "smbumount \"" + strmnt_dir + "\"";
            string strsmbmount_out = "";
            int intsmbmount_ret = system_capture_out(strCMD, strsmbmount_out);
            // Replace all occurances of newline in the output...
            strsmbmount_out=replace(strsmbmount_out, "\n", ". ");

            // Check the smbmount output and return lines:
            if (intsmbmount_ret != 0 || strsmbmount_out != "") {
              log_error("Could not unmount directory " + strmnt_dir + ". " + strsmbmount_out);
            } // END: if (intsmbmount_ret != 0 || strsmbmount_out != "")
            else {
              // The smbumount was successful. Loop again (check if there is a service mounted here, unmount it)
              blncheck_mount_again = true;
            }
          } // END: if (get_first_mounted_samba_service(strmnt_dir) != "")
        } while (blncheck_mount_again);

        // Now attempt to remove the mount subdir:
        rmdir(strmnt_dir); // Throws an exception if there is an error.
      } // END: while (strmnt != "")

      // Attempt to remove the PID subdirectory:
      try {
        rmdir(strpid_dir);
      }

      catch(...) { // If rmdir throws an exception then log a warning and continue...
        log_warning("Unable to remove directory " + strpid_dir);
      }
    } // END: if (!blnpid_found)
  } // END: while (strpid != "") {

  // Are there any subdirectories left under the base mount directory?
  dir_list base_remaining_subdirs(strbase_dir, "", DT_DIR);
  if (base_remaining_subdirs.size() == 0) {
    // No subdirectories remain. Remove the base mount directory:
    try {
      rmdir(strbase_dir);
    } catch(...) {} // Don't do anything if this rmdir fails. Probably /mnt/ is only root-writable.
  } // END: if (base_remaining_subdirs.count() == 0)
}

string windows_mount::get_first_mounted_samba_service(const string & strpath) {
  // Return the first SMB resource that is mounted under the specified directory. If there are no resources mounted
  // there, then return an empty string.
  string strret = ""; // The return code
  // Make sure the path does not have a / on the end:
  string strmount_path = strpath;
  if (right(strmount_path,1) == "/") {
    strmount_path = substr(strmount_path, 0, strmount_path.length() - 1);
  }

  // Generate the command to run:
  string strcmd = "cat /etc/mtab | awk '{if ($2 == \"" + strmount_path + "\" && $3==\"smbfs\") { print $1; exit} }'";

  // A variable to store the command output:
  string strcmd_out = "";

  // Now run the command and fetch the output:
  int intret = system_capture_out(strcmd, strcmd_out);

  if (intret != 0) {
    // Unexpected result code! Throw an exception!
    my_throw("Unexpected return code!");
  }

  // If /etc/mtab did not return a result, then try a ps aux:
  if (strcmd_out == "") {
    strcmd = "ps auxw | awk '{if($11 == \"smbmount\" && $13==\"" + strmount_path + "/\") {print $12; exit}}'";
    intret = system_capture_out(strcmd, strcmd_out);

    if (intret != 0) {
      // Unexpected result code! Throw an exception!
      my_throw("Unexpected return code!");
    }
  }

  // We now have the samba resource (if any) that was mounted here:
  strret = strcmd_out;

  return strret;
}

#endif // END: #ifdef __linux__
