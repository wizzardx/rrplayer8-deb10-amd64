
#include "mp3_tags.h"
#include "string_splitter.h"
#include <stdio.h>      // I/O (file remove() function)
#include <fstream>
#include "file.h"
#include "exception.h"
#include "my_string.h"
#include "system.h"

mp3_tags::mp3_tags(){
  // Initialize internal vars to empty.
  strTagBufferFile = "";
  blnTagDetailsChanged = false;
}

mp3_tags::~mp3_tags(){
}

void mp3_tags::init(const string & strBuffFilePath) {  // Initialize the mp3_tags object, provide a buffer filename
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
  int intlineno=0;
  while (tag_list_file.getline(ch_FileLine, 2047)) {
    ++intlineno;
    string strLine = ch_FileLine;

    // Cut out any trailing CR
    strLine = remove_last_char(strLine, '\r');

    // Break the line into fields: mp3_path||mp3_size||mp3_artist||mp3_album||mp3_trackname||mp3_length
    string strmp3_path, strmp3_size, strmp3_artist, strmp3_album, strmp3_trackname;
    int intmp3_length;

    // Parse the line read in.
    string_splitter split(strLine, "||");
    if (split.size() != 6) {
      // Move the file to a backup location (ie, delete, but keep a copy around for reference.
      mv(strTagBufferFile, strTagBufferFile + ".old");
      my_throw("Invalid number of fields (" + itostr(split.size()) + ") found in " + strTagBufferFile + ", line " + itostr(intlineno) + ". File was moved to: " + strTagBufferFile + ".old");
    }
    strmp3_path      = trim(split);
    strmp3_size      = trim(split);
    strmp3_artist    = trim(split);
    strmp3_album     = trim(split);
    strmp3_trackname = trim(split);
    intmp3_length    = -1;
    try {
      intmp3_length  = strtoi(trim(split));
    } catch_exceptions;

    // Now that we have extracted the mp3 file details, check
    //     1) that it is not already in the collection
    //     2) That the file exists on the hard-drive, and
    //     3) The listed filesize is correct.
    if (!file_tag_cached(strmp3_path)) {
      if (file_exists(strmp3_path)) {
        long lngFileSize = strtol(strmp3_size);
        if ((lngFileSize > 0) && (lngFileSize==file_size(strmp3_path))) {
          // The listed file exists and is the same size as listed in the buffer file. Add it to the memory listing
          cache_file_tag(strmp3_path, lngFileSize, strmp3_artist, strmp3_album, strmp3_trackname, intmp3_length);
        }
      }
    }
  }
  tag_list_file.close();
}

string mp3_tags::get_mp3_description(const string & strFilePath) {
  // Get an MP3's tag and return a description from the tags (Artist - Track title)
  // Throws an exception if there is a problem, eg: no tag in the MP3. The calling
  // function should detect this and use the filename if the tag could not be retrieved.

  // Fetch filename minus extension, and the extension:
  string strfname_no_ext = "";
  string strext = "";
  {
    string strfile = get_short_filename(strFilePath);
    string_splitter fname_split = string_splitter(strfile, ".");
    strext = lcase(fname_split[fname_split.size() - 1]);
    strfname_no_ext = substr(strfile, 0, strfile.length() - strext.length() - 1);
  }

  // Default return value is the filename with no extension:
  string strret = strfname_no_ext;

  // Check if the fil e exists (but not for entries that
  // represent cd audio:
  if (!file_exists(strFilePath) &&
      strext != "cda" &&
      strext != "cdr" &&
      strFilePath != "LineIn" &&
      strFilePath != "/dev/cdrom") my_throw("File not found: " + strFilePath);

  // If this is an mp3 file, then attempt to fetch mp3 tag details:
  if (strext == "mp3") {
    // Declare some variables, populated later in the function:
    string strAlbum     = "";
    string strArtist    = "";
    string strTrackName = "";

    // Get a cached entry (either from memory or generate it)
    tblmp3_info_map::const_iterator item;
    item = get_mp3_info_item(strFilePath);

    // Fetch details from the cached tag info:
    strAlbum     = (*item).second.strAlbum;
    strArtist    = (*item).second.strArtist;
    strTrackName = (*item).second.strTrackName;

    // Now generate and return the description:
    if (strTrackName != "") {
      // Track name is defined.
      strret = strTrackName;
      // Is the artist defined?
      if (strArtist != "") {
        strret = strArtist + " - " + strret;
      }
      // Otherwise, is the album defined?
      else if (strAlbum != "") {
        strret = strAlbum + " - " + strret;
      }
    }
  }

  return strret;
}

int mp3_tags::get_mp3_length(const string & strFilePath) {
  // Fetch the length of an mp3.

  // Get a cached entry for the file (either from memory or generate it):
  tblmp3_info_map::const_iterator item = get_mp3_info_item(strFilePath);

  // Return the length:
  return (*item).second.intLength;
}

string mp3_tags::get_mp3_artist(const string & strFilePath) {
  // Fetch an MP3's artist

  // Get a cached entry for the file (either from memory or generate it):
  tblmp3_info_map::const_iterator item = get_mp3_info_item(strFilePath);

  // Return the length:
  return (*item).second.strArtist;
}

string mp3_tags::get_mp3_album(const string & strFilePath) {
  // Fetch an MP3's album.

  // Get a cached entry for the file (either from memory or generate it):
  tblmp3_info_map::const_iterator item = get_mp3_info_item(strFilePath);

  // Return the length:
  return (*item).second.strAlbum;
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
                      << (*item).second.strTrackName << "||"
                      << (*item).second.intLength << "||"
                      << endl;
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

void mp3_tags::cache_file_tag(const string & strFile, const long lngFileSize, const string & strArtist, const string & strAlbum, const string & strTrackName, const int intTrackLength) {
  // Store mp3 tag details in memory.

  // Remember to re-write the tag file later:
  blnTagDetailsChanged = true;

  // Now get a pointer to either a) A new tag record, or b) An existing tag record;
  tblmp3_info * tag_info = &mp3_info[strFile];

  // Now update the fields
  tag_info->strMP3Path = strFile;
  tag_info->lngFileSize = lngFileSize;
  tag_info->strArtist = strArtist;
  tag_info->strAlbum = strAlbum;
  tag_info->strTrackName = strTrackName;
  tag_info->intLength = intTrackLength;
}

bool mp3_tags::file_tag_cached(const string & strFile) {
  // Return true if a file is listed in the in-memory tag collection.
  tblmp3_info_map::const_iterator item;
  item = mp3_info.find(strFile);
  return (item != mp3_info.end());
}

tblmp3_info_map::const_iterator mp3_tags::get_mp3_info_item(const string & strFilePath) {
  // Fetch the cached item. If it doesn't exist then load it

  // Freak out if the file is not an mp3, or if it doesn't exist:
  if (lcase(get_file_ext(strFilePath)) != "mp3") my_throw("File is not an mp3, can't fetch info for it: " + strFilePath);
  if (!file_exists(strFilePath)) my_throw("File not found: " + strFilePath);

  // Check if the MP3 info is already loaded into memory, return it if so:
  tblmp3_info_map::const_iterator item;
  item = mp3_info.find(strFilePath);
  if (item != mp3_info.end()) return item;

  // MP3 info is not in memory, attempt to load it via mp3info:

  // Check if mp3info is installed
  if (!file_exists("/usr/bin/mp3info")) my_throw("mp3info not installed!");

  // - Generate the command
  string strCmd="/usr/bin/mp3info -p \"Artist:%a\\nAlbum:%l\\nTrack:%t\\nLength:%S\\n\" " + string_to_unix_filename(strFilePath) + "";

  // - Run the command
  string stroutput;
  system_capture_out(strCmd, stroutput);

  // Process the output:
  string_splitter mp3info_split(stroutput, "\n");

  // Should be 4 or 5 lines (4=no problem, 5=warning, no ID3 1.x tag).
  if (mp3info_split.size() != 4 && mp3info_split.size() != 5) my_throw("Detected in the mp3info output! Invalid line count. Output follows:\n" + stroutput);

  // Variables to be extracted:
  string strArtist, strAlbum, strTrackName;
  int intLength = -1;

  // Now parse the lines:
  while (mp3info_split) {
    string strline = mp3info_split;
    // Ignore lines which end with "does not have an ID3 1.x tag"
    string strerror_end = "does not have an ID3 1.x tag.";
    if (right(strline, strerror_end.length()) != strerror_end) {
      // Fetch the part before and after the colon:
      string_splitter line_split(strline, ":");
      string strfield = line_split;
      string strvalue = substr(strline, strfield.length() + 1);
      // Check which field we have:
      if (strfield == "Artist") {
        strArtist = strvalue;
      }
      else if (strfield == "Album") {
        strAlbum = strvalue;
      }
      else if (strfield == "Track") {
        strTrackName = strvalue;
      }
      else if (strfield == "Length") {
        intLength = -1;
        try {
          intLength = strtoi(strvalue);
        } catch_exceptions;
      }
      else log_warning("Found an unknown field in the mp3info output! " + strfield);
    }
  }

  // - Update the in-memory collection
  cache_file_tag(strFilePath, file_size(strFilePath), strArtist, strAlbum, strTrackName, intLength);

  // - Now fetch & return the cached item:
  item = mp3_info.find(strFilePath);
  if (item == mp3_info.end()) LOGIC_ERROR; // We just cached the item, we should never not find it!
  return item;
}
