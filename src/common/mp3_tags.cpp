/***************************************************************************
                          mp3_tags.cpp  -  description
                             -------------------
    begin                : Wed Jun 11 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#include "mp3_tags.h"
#include "string_splitter.h"
#include <stdio.h>      // I/O (file remove() function)
#include <fstream>
#include "file.h"
#include "exception.h"
#include "my_string.h"

mp3_tags::mp3_tags(){
  // Initialize internal vars to empty.
  strTagBufferFile = "";
  blnTagDetailsChanged = false;  
}

mp3_tags::~mp3_tags(){
}

void mp3_tags::init(const string strBuffFilePath) {  // Initialize the mp3_tags object, provide a buffer filename  
  // Initialise the mp3_tags object
  // Check if mp3info is installed on the machine
  if (!file_exists("/usr/bin/mp3info")) {
    my_throw("The mp3info tool is not installed!");
  }

  // Read any previously-buffered details into memory
  strTagBufferFile = strBuffFilePath;
  
  // Tags buffer file exists?
  if (!file_exists(strTagBufferFile)) return;
  
  ifstream tag_list_file(strTagBufferFile.c_str());
  if (!tag_list_file) {
    my_throw("Unable to open " + strTagBufferFile);
  }

  char ch_FileLine[2048] = "";
  while (tag_list_file.getline(ch_FileLine, 2047)) {
    string strLine = ch_FileLine;

    // Cut out any trailing CR
    strLine = remove_last_char(strLine, '\r');

    // Break the line into fields: mp3_path||mp3_size||mp3_artist||mp3_album||mp3_trackname
    string strmp3_path, strmp3_size, strmp3_artist, strmp3_album, strmp3_trackname;

    // Parse the line read in.
    string_splitter split(strLine, "||", "!!");
    strmp3_path      = trim(split);
    strmp3_size      = trim(split);
    strmp3_artist    = trim(split);
    strmp3_album     = trim(split);
    strmp3_trackname = trim(split);
    string strEND    = split; // Make sure we have the correct # of elements.
    if (strEND != "!!") {
      my_throw("Bad output returned from mp3info!");
    }
    
    // Now that we have extracted the mp3 file details, check
    //     1) that it is not already in the collection
    //     2) That the file exists on the hard-drive, and
    //     3) The listed filesize is correct.
    if (!file_tag_cached(strmp3_path)) {
      if (file_exists(strmp3_path)) {
        long lngFileSize = strtol(strmp3_size);
        if ((lngFileSize > 0) && (lngFileSize==file_size(strmp3_path))) {
          // The listed file exists and is the same size as listed in the buffer file. Add it to the memory listing
          cache_file_tag(strmp3_path, lngFileSize, strmp3_artist, strmp3_album, strmp3_trackname);
        }
      }
    }
  }
  tag_list_file.close();
}

string mp3_tags::get_mp3_description(const string strFilePath) {
  // Get an MP3's tag and return a description from the tags (Artist - Track title)
  // Throws an exception if there is a problem, eg: no tag in the MP3. The calling 
  // function should detect this and use the filename if the tag could not be retrieved.
  string strRetstr = "";

  // Check if the file exists
  if (file_exists(strFilePath)) {
    // Check if the description is already loaded into memory
    tblmp3_info_map::const_iterator item;
    item = mp3_info.find(strFilePath);
    if (item == mp3_info.end()) {
      // MP3 description is not loaded into memory - run mp3info

      // Define and also clear out the line vars.
      string strArtist, strAlbum, strTrackName;
      strArtist = strAlbum = strTrackName = "";  // Done in separate statements, because if on same line,
                                                                        // the variables will = "" at declaration but not be cleared after
      // - Remove any existing mp3info.txt
      remove("mp3info.txt");
      // Check if mp3info is installed
      if (file_exists("/usr/bin/mp3info")) {
        // - Generate the command
        string strCmd="/usr/bin/mp3info -p \"Artist:%a\\nAlbum:%l\\nTrack:%t\\n\" \"" + strFilePath + "\" &> mp3info.txt";
        // - Run the command
        if (system(strCmd.c_str()) == 0) {
          // - Open mp3info.txt
          ifstream mp3info("mp3info.txt");
          if (!mp3info) {
            // Could not open mp3info.txt
            my_throw("Could not open mp3info.txt");
          }
          else {
            // mp3info is now open. Read the lines
            int intLineNum = 0;
            char ch_FileLine[2048] = "";
            string strLine = "";

            bool blnQuitFile=false; // If an anticipated error occurs then quit scanning the file
            blnQuitFile=false;

            while (mp3info.getline(ch_FileLine, 2047) && !blnQuitFile) {
              // Read in the file line
              ++intLineNum;
              strLine=trim(ch_FileLine);

              // Check the output log for possible mp3info errors.
              blnQuitFile=right(strLine, 29) == "does not have an ID3 1.x tag.";

              if (!blnQuitFile) {
                switch (intLineNum) {
                  case 1: {

                    // This line should start with "Artist:"
                    if (substr(strLine, 0, 7) == "Artist:") {
                      strArtist = substr(strLine, 7);
                    }
                    else my_throw("Invalid 1 line read from mp3info.txt: " + strLine);
                    break;
                  }
                  case 2: {
                    // This line should start with "Album:"
                    if (substr(strLine, 0, 6) == "Album:") {
                      strAlbum = substr(strLine, 6);
                    }
                    else my_throw("Invalid line 2 read from mp3info.txt: " + strLine);
                    break;
                  }
                  case 3: {
                    // This line should start with "Track:"
                    if (substr(strLine, 0, 6) == "Track:") {
                      strTrackName = substr(strLine, 6);
                    }
                    else my_throw("Invalid line 3 read from mp3info.txt: " + strLine);
                    break;
                  }
                  default: my_throw("Too many lines read from mp3info.txt");
                }
              }
            }
            // All lines read. Close the file and delete it
            mp3info.close();
            remove("mp3info.txt");
            // - Update the in-memory collection (also mark that a change has happened)
            cache_file_tag(strFilePath, file_size(strFilePath),strArtist, strAlbum, strTrackName, true);
            // - Generate the mp3 description.
            if (strArtist != "" && strTrackName != "") {
              strRetstr = strArtist + " - " +strTrackName;
            }
          }
        } else {
          // System instruction was unsuccessful
          my_throw("There was an error shelling " + strCmd);
        }
      }
      else {
        // mp3info not installed
        my_throw("mp3info not installed!");
      }
    }
    else {
      // Description is loaded in memory. So now we return the pre-loaded details.
      string strArtist = (*item).second.strArtist;
      string strTrackName = (*item).second.strTrackName;
      // - Generate the mp3 description.
      if (strArtist != "" && strTrackName != "") {
        strRetstr = strArtist + " - " + strTrackName;
      }
    }
  } else my_throw(strFilePath + " not found."); //  Mp3 to fetch tag from does not exist.
                                                // Could be because of an XMMS freeze 
                                                // or because XMMS does not yet 
                                                // have a title ready?
  return strRetstr;
}

void mp3_tags::save_changes() {
  // Rewrite the buffer file if there are any new tag details that have been loaded.
  if (blnTagDetailsChanged) {
    ofstream buffer_file(strTagBufferFile.c_str());
    if (buffer_file) {
      // Buffer file has been opened for writing. Start writing
      tblmp3_info_map::iterator item = mp3_info.begin();
      while (item!=mp3_info.end()) {
        // Check if the file exists and is the same size as listed in memory.
        bool blnSave = false; // Do we save this entry?
        if (file_exists((*item).second.strMP3Path)) {
          blnSave = file_size((*item).second.strMP3Path) == (*item).second.lngFileSize;
        }
        if (blnSave) {
          // Save the line to the file
          buffer_file << (*item).second.strMP3Path << "||"
                          << (*item).second.lngFileSize << "||"
                          << (*item).second.strArtist << "||"
                          << (*item).second.strAlbum << "||"
                          << (*item).second.strTrackName << endl;
        }
        else {
          // Do not save the line to the file. Also remove the line details from the collection
          // - Internal collection is changing, but we have just skipped saving the
          //   invalid MP3 entry to the file anyway.
          mp3_info.erase(item);
        }
        ++item;
      }
      blnTagDetailsChanged = false;
    }
    else my_throw("Could not open " + strTagBufferFile + " for writing.");
  }
}

// ************************************************
// Internal caching functions....
// ************************************************

void mp3_tags::cache_file_tag(const string strFile, const long lngFileSize, const string strArtist, const string strAlbum, const string strTrackName, const bool blnNewEntry) {
  // Store mp3 tag details in memory.

  // A change to the collection is made, remember to update the buffer file later.
  if (blnNewEntry) blnTagDetailsChanged = true;

  // Now get a pointer to either a) A new tag record, or b) An existing tag record;
  tblmp3_info * tag_info = &mp3_info[strFile];

  // Now update the fields
  tag_info->strMP3Path = strFile;
  tag_info->lngFileSize = lngFileSize;
  tag_info->strArtist = strArtist;
  tag_info->strAlbum = strAlbum;
  tag_info->strTrackName = strTrackName;
}

bool mp3_tags::file_tag_cached(const string strFile) {
  // Return true if a file is listed in the in-memory tag collection. 
  tblmp3_info_map::const_iterator item;
  item = mp3_info.find(strFile);
  return (item != mp3_info.end());
}
