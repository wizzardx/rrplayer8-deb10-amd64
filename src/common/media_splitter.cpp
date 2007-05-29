/***************************************************************************
                          media_splitter.cpp  -  description
                             -------------------
    begin                : Tue Sep 23 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifdef __linux__

#include "media_splitter.h"
#include "logging.h"
#include "mp3_handling.h"
#include "string.h"
#include "exception.h"
#include "file.h"
#include "system.h"
#include <sys/stat.h>

// Constructor

media_splitter::media_splitter(const string & strBodyFile_, const string & strTempDir_) {
  strBodyFile = strBodyFile_;
  strTempDir = strTempDir_;
  strTempDir = ensure_last_char(strTempDir, '/');

  // Clear the split-related directories and variables..
  split_reset();

  // Now check the args:
  if (strTempDir == "")          my_throw("Temporary directory not set");
  if (!dir_exists(strTempDir))   my_throw("Temporary directory \"" + strTempDir + "\" not found");
  if (strBodyFile == "")         my_throw("Body file not set");
  if (!file_exists(strBodyFile)) my_throw("Body file \"" + strBodyFile + "\"not found!");
}

// Destructor

media_splitter::~media_splitter() {
  // Clear the split-related directories and variables..
  split_reset();
}

// Perform the split operation

void media_splitter::split() {
  // Clear the directories and variables initialized by split...
  split_reset();

  // Copy the Body to the temporary directory, and strip tag data from it.
  string strTempBodyFile = strTempDir + "body.mp3"; // We're using MP3s for now...

  // Attempt to copy the body file to the temporary directory and filename:.
  cp(strBodyFile, strTempBodyFile); // Throws an exception if there is a failure.

  // Make the file writable:
  CHECK_LIBC(chmod(strTempBodyFile.c_str(), 0664), strTempBodyFile);

  // Remove tags which give quelcom trouble..
  mp3_strip_tags(strTempBodyFile);

  // Check additional mp3 specs which give quelom problems:
  check_mp3_specs(strTempBodyFile);

  // Break the body file into sections
  extract_mp3s(strTempBodyFile, strTempDir, &split_info.blnSpaceAtStart, &split_info.blnSpaceAtEnd);

  // Count the number of files extracted
  split_info.intNumExtracted = count_dir_files(strTempDir) -1;
  if (split_info.intNumExtracted < 1) {
    // There was a problem counting the number of extracted files!
    LOGIC_ERROR;
  }

  // Calculate the total number of spaces
  split_info.intNumSpacesFound = split_info.intNumExtracted - 1;
  if (split_info.blnSpaceAtStart) ++split_info.intNumSpacesFound;
  if (split_info.blnSpaceAtEnd) ++split_info.intNumSpacesFound;
}

void media_splitter::split_reset() {
  // Clear the directories and variables created by split()
  // - Clear the directory contents.
  if (strTempDir != "") {
    string strCMD = "rm -f " + string_to_unix_filename(strTempDir) + "*"; // Not a recursive delete - not needed.
    if (system(strCMD.c_str()) != 0) {
        my_throw("Error clearing temporary directory!");
    }
  }

  // - Clear the variables
  split_info.intNumExtracted = 0;
  split_info.intNumSpacesFound = 0;
  split_info.blnSpaceAtEnd = false;
  split_info.blnSpaceAtEnd = false;
};

// Return information about the parts and spaces:

int media_splitter::get_num_parts() {
  // Return the number of extracted parts
  return split_info.intNumExtracted;
}

int media_splitter::get_num_spaces() {
  // Return the number of spaces discovered.
  return split_info.intNumSpacesFound;
}

bool media_splitter::space_at_begin() {
  // Was there a space at the very beginning of the body?
  return split_info.blnSpaceAtStart;
}

bool media_splitter::space_at_end() {
  // Was there a space at the very end of the body?
  return split_info.blnSpaceAtEnd;
}

string media_splitter::get_part_file(const int intpart_num) {
  // Return the path and name to an extracted file.

  // Is the requested filename within the range of file numbers that were extracted?
  if ((intpart_num >= 1) && (intpart_num <= split_info.intNumExtracted)) {
    // Does the file exist?
    string strFileName = strTempDir + itostr(intpart_num) + ".mp3";
    if (file_exists(strFileName)) {
      // The extracted file exists, so return it.
      return strFileName;
    }
    else {
      // The media part was not found! Possibly it got deleted.
      my_throw("The requested extracted Body part could not be found!");
    }
  }
  else {
    // A file was requested that was outside the range of extracted body parts!
    LOGIC_ERROR;
  }
}

int media_splitter::get_space_type(const int intspace_num) {
  // Fetch the type of a given space...
  // For the present, all spaces are considered as type 0.
  if ((intspace_num >= 1) && (intspace_num <= split_info.intNumSpacesFound)) {
    return 0;
  }
  else {
    // A space was requested that was outside the range of discovered spaces!
    LOGIC_ERROR;
  }

  return 0;
}

// Check additional mp3 specs which give quelom problems:
void media_splitter::check_mp3_specs(const string & strBodyFile) {
  // Check that mp3info exists:
  if (!file_exists("/usr/bin/mp3info")) my_throw("mp3info not installed!");

  // Fetch the technical specs we're interested in:
  string stroutput = "";
  system_capture_out_throw("/usr/bin/mp3info -p '%q\n' " + strBodyFile, stroutput);

  if (stroutput != "44") my_throw("Invalid mp3 sampling frequency " + stroutput + " kHz. 44 kHz required: " + strBodyFile);
}

#endif
