/***************************************************************************
                          windows_mount.h  -  description
                             -------------------
    begin                : Tue Oct 7 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifdef __linux__ // windows_mount support is only available for Linux...

#ifndef WINDOWS_MOUNT_H
#define WINDOWS_MOUNT_H

/**
  *@author David Purdy
  */

#include <string>
#include "rr_utils.h"

using namespace std;

// This class lets you to make local SMB mounts to windows-style network paths,
// eg: \\lnxfileserver\network\david\work\moo. The mounts are automatically removed
// when the object is destroyed. Also, if the mount already exists (ie another windows_mount
// object is active for this program instance), then this class re-uses the mount - this
// means that the minimum possible # of mounts are created, and you can create windows_mount
// objects in loops and sub-functions without worrying about smbmount being called for
// every new windows_mount object.

class windows_mount {
public:
  // Constructor
  // - strmnt_name is a brief *unique* description for the share.
  //   It will be used to generate the mount point
  windows_mount(const string & strWindowsPath, const string & strlogin_user, const string & strencrypted_login_password, const string & strmnt_descr_, const unsigned long long lngmin_free_space);

  // Destructor
  ~windows_mount();

  // If the construction succeeded, return the local path where the windows
  // directory was mounted to..
  string local_path() const;      // Return the path to access the mounted sub-directory under.
  string remote_path() const;  // Return the original windows sub-directory that was mounted.
private:
  // do_mount - called by the constructor, because kdevelop can't catch breakpoints in constructors!
  void do_mount(const string & strWindowsPath, const string & strlogin_user, const string & strencrypted_login_password, const string & strmnt_descr_, const unsigned long long lngmin_free_space);
  void do_unmount(); // Called by the destructor to umount a mounted share (only if the object actually created the mount)
  
  void tidy_old_mounts(); // Remove any mount directories created by program instances that are no longer running.

  string get_first_mounted_samba_service(const string & strpath); // Search for and return the first samba resource mounted on this directory.


  string strmnt_descr; // A brief description of the mount, eg "SNS backup"
  
  // Directories involved with the mounting:
  string strwindows_path; // eg: "\\mail\data_whouse\Scheduling\Scheduling Backup\SNS Files\"
  string strmount_base; // eg: ...progs/schedmon/mnt/
  string strmount_pid; // eg: ...progs/schedmon/mnt/1234/  <- PID number of this program instance.
  string strmount_dir; // eg: ...progs/schedmon/mnt/1234/mail_data_whouse/ <- A directory for the mount, based on the name of the service
  string strfull_path;  // eg: ...progs/schedmon/mnt/1234/mail_data_whouse/Scheduling/Scheduling Backup/SNS Files/ <- The directory the user is interested in, under the mounted directory

  bool blncreated_mount; // Later objects attempt to reuse mounts created by earlier object instances.
                                // When the first object is destroyed, it will unmount the share. By that time, later
                                // objects (which may have borrowed the share) should also have been destroyed.
                                // - This is so that we can call smbmount a minimum number of times in a given app instance.
};



#endif
#endif // END: #ifdef __linux__
