/***************************************************************************
                          build_num.cpp  -  description
                             -------------------
    version              : v0.05
    begin                : Fri Aug 29 2003
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

#include "build_num.h"
#include "rr_utils.h"

using namespace rr;

/********************************************************************************
Code for maintaining and returning a "Build Number" for the project.
*********************************************************************************/

// String constants for updating the Makefile to generate

const string strBuildNumScript = "make_all_with_build_num.script";

const string strFind1="DEFS =";
const string strReplace1[3] = {"BUILD_NUM=-1",
                                      "DEFS = -DHAVE_CONFIG_H -DBUILD_NUM=$(BUILD_NUM)",
                                      ""};

const string strFind2="all:";
const string strReplace2[3] = {"all:",
                                                "\t@./" + strBuildNumScript + " $(PACKAGE)",
                                                 ""};
                                                 
bool MakeFileUpdatedForBuildNum(const string strMakefilePath) {
  // If MakeFile exists in a directory, then check for the lines that are required for build numbers to be maintained

  // The function "FindTextInFile" also returns false if a file does not exist.
  return (((FindTextInFile(strReplace1[0], strMakefilePath)) &&
         (FindTextInFile(strReplace1[1], strMakefilePath)) &&
         (FindTextInFile(strReplace2[0], strMakefilePath)) &&
         (FindTextInFile(strReplace2[1], strMakefilePath))));
}

int GetBuildNum() {
  bool blnDefined = false;

  #ifdef BUILD_NUM
    // This code will run if BUILD_NUM was #defined by the Makefile  
    blnDefined=true;
    int intBuildNum = BUILD_NUM;
  #else
    // This code will run if BUILD_NUM was not #defined by the Makefile
    #warning =====================================================
    #warning BUILD_NUM not defined, this means your Makefiles were reset.
    #warning
    #warning -> Please run the project (and call GetBuildNum()) to update the
    #warning      Makefiles, and then compile the project again.
    #warning =====================================================
  
    blnDefined=false;
    int intBuildNum = -1;
    log_message("BUILD_NUM not defined, will update Makefile...");
  #endif

  // Fetch the path where this process is executing from
  string strExecDir = ""; // The directory this process is executingin
  string strDummy = ""; // Value we're not interested in (name of running executable)

  break_down_file_path(GetExecPath(), strExecDir, strDummy);

  // Check the returned execution directory
  if (strExecDir=="") {
    log_error("Could not retrieve execution directory!");
    return -1;     
  }                
  
  // If BUILD_NUM is defined, then check for the existance of Makefile. If the file exists then check that
  // it is correctly updated.
  string strMakefilePath = strExecDir + "Makefile";
  bool blnUpdateMakefile = false; // This is set to true if the Makefile is to be updated.
  
  if (blnDefined) {
    if (FileExists(strMakefilePath)) {
      // Makefile exists, assume that the project is being developed... Is the Makefile correct?
      if (!MakeFileUpdatedForBuildNum(strMakefilePath)) {
        // Makefile is not currently configured to maintain a build number. Update the makefile!
        log_message("Makefile is not configured for Build Numbers! Will update Makefile...");
        blnUpdateMakefile = true;
      }                               
    }                
  }
  else {
    // BUILD_NUM is not defined, so we need to update Makefile...
    blnUpdateMakefile = true;
  }
  
  // Do we need to update the Makefile?
  if (blnUpdateMakefile) {
    // Check that some files required for the build numbers update process exist...

    // Update Makefile if it exists in the player's path.
    log_message("**** UPDATING Makefile ****");
      
    // Does the Makefile exist?
    if (!FileExists(strMakefilePath)) {
      log_error("Could not find the project Makefile.");
      return -1;
    }

    // Does the script called by the Makefile exist?
    if (!FileExists(strExecDir + strBuildNumScript)) {
      log_error("Could not find " + strBuildNumScript);
      return -1;
    }

    // Does the text file containing the build number (and maintained by the script) exist?
    if (!FileExists(strExecDir + "build_num.txt")) {
      log_error("Could not find build_num.txt");
    }

    // A final check: Make sure that there is not already an 'all_old:'Target in the Makefile.
    // - If there is already one, then this modification will add a second one, which could could
    // cause an endless recursion situation.
    if (FindTextInFile("all_old:", strMakefilePath)) {
      // all_old: was already in the makefile! Issue a warning
      log_error(string("*** ERROR: the 'all_old:' target already exists in Makefile! Please check the logic in ") + __FILE__);
      return -1;
    } // end if

    // The all_old target was not found. Proceed with the update.

    // Update the Makefile - run AWK to do the update...
    string strcmd = "cat \"" + strMakefilePath + "\" | "
       "awk "
         "'{ if (substr($0,1," + itostr(strFind1.length()) + ")==\"" + strFind1+ "\") "
            "{"
               "print \"" + strReplace1[0] + "\"; "
               "print \"" + strReplace1[1] + "\"; "
               "print \"" + strReplace1[2] + "\"; "
            "} else if (substr($0,1," + itostr(strFind2.length()) + ")==\"" + strFind2 + "\") "
            "{"
               "print \"" + strReplace2[0] + "\"; "
               "print \"" + strReplace2[1] + "\"; "
               "print \"" + strReplace2[2] + "\"; "
               "print \"all_old: \" substr($0, 6); "
            "} else {print $0 }"
         "}'"
         " > \"" + strExecDir + "Makefile_new.txt\"; mv \"" + strExecDir + "Makefile_new.txt\" \"" + strMakefilePath +
         "\"; touch \"" + strExecDir + "build_num.cpp\""; // Also make sure that this code is rebuilt, ie that
                                                                                  //BUILD_NUM becomes available. Otherwise we
                                                                                  // may end up updating the Makefile every time!

    // Check if there was an error.
    if (system(strcmd.c_str()) != 0) {
      log_error("There was an error updating the Makefile!");
      return -1;
    }

    // Check if the new lines exist in the makefile.
    if (!MakeFileUpdatedForBuildNum(strMakefilePath)) {
      log_error("The Makefile was not updated correctly!");
      return -1;
    }

    // The Makefile update is complete. Ask the user to recompile the project, this action will process the new Makefile.
    log_message("**** Makefile was updated. Please recompile your project ****");
    return -1;
  } // end if

  // Makefile was not updated, just return the build number.
  return intBuildNum;    
} // end function
