/***************************************************************************
                          media_splitter.cpp  -  description
                             -------------------
    begin                : Tue Sep 23 2003
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

#ifdef __linux__

#include "media_splitter.h"
#include "rr_utils.h"
#include "logging.h"
#include "mp3_handling.h"

using namespace rr;

// Constructor

media_splitter::media_splitter(const string & strBodyFile_, const string & strTempDir_) {
  strBodyFile = strBodyFile_;
  strTempDir = strTempDir_;
  strTempDir = ensure_char_at_end(strTempDir, '/');

  // Clear the split-related directories and variables..
  split_reset();
}

// Destructor

media_splitter::~media_splitter() {
  // Clear the split-related directories and variables..
  split_reset();
}                                  

// Boolean operator

media_splitter::operator bool() {
  // Check if the temporary directory exists, and if the body file exists...

  // Temporary directory set?
  if (strTempDir == "") {
    rr_throw("Temporary directory not set");
  }



  // Temporary directory exists?
  if (!DirExists(strTempDir)) {
    rr_throw("Temporary directory \"" + strTempDir + "\" not found");
  }

  // Body file set?
  if (strBodyFile == "") {
    rr_throw("Body file not set");
  }

  // Body file exists?
  if (!FileExists(strBodyFile)) {
    rr_throw("Body file \"" + strBodyFile + "\"not found!");
  }

  return true;
}

// Perform the split operation

void media_splitter::split() {
  bool blnError = false; // Set to true when there is an error;
                                    // - Used to avoid hugely-nested IF statements, at the
                                    //    cost of this variable being checked at each if.

  // Is the object OK to be used? (ie all required fields set)

  if (!this) {
    // The object is not yet ready to be used!
    // ** Actually, this code will never be called, because the bool operator throws an exception
    // if there are any problems.
    blnError = true;
    rr_throw("Internal Logic Error! This media_splitter object is not ready to be used!");
  } // end else

  // Clear the directories and variables initialized by split...
  split_reset();

  // Copy the Body to the temporary directory, and strip tag data from it.
  string strTempBodyFile = strTempDir + "body.mp3"; // We're using MP3s for now...

  // Attempt to copy the body file to the temporary directory and filename:.
  cp(strBodyFile, strTempBodyFile); // Throws an exception if there is a failure.

  // Remove tags which give quelcom trouble..
  MP3_StripTagData(strTempBodyFile);
  
  // Break the body file into sections
  ExtractMP3s(strTempBodyFile, strTempDir, &split_info.blnSpaceAtStart, &split_info.blnSpaceAtEnd);

  // Count the number of files extracted
  split_info.intNumExtracted = CountDirFiles(strTempDir) -1;
  if (split_info.intNumExtracted < 1) {
    // There was a problem counting the number of extracted files!
    rr_throw("Internal Logic Error: I counted " + itostr(split_info.intNumExtracted) + " files extracted from the Body!");
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
        rr_throw("Error clearing temporary directory!");
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
    if (FileExists(strFileName)) {
      // The extracted file exists, so return it.
      return strFileName;
    }
    else {
      // The media part was not found! Possibly it got deleted.
      rr_throw("The requested extracted Body part could not be found!");
    }
  }
  else {
    // A file was requested that was outside the range of extracted body parts!
    rr_throw("Internal Logic Error - media_splitter object was asked for the filename of a non-existant extracted Body part!");
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
    rr_throw("Internal Logic Error - media_splitter object was asked for the type of a non-existant extracted Body space!");
  }

  return 0;
}

#endif

