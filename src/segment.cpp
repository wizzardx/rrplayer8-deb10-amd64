#include "segment.h"
#include "common/testing.h"
#include "common/file.h"
#include "common/dir_list.h"
#include "common/my_string.h"
#include <fstream>

using namespace std;

// Constructor
segment::segment() {
  reset();
}

// Destructor
segment::~segment() {
}

// Reset all segment info
void segment::reset() {
  blnloaded = false;
  
  // Information about the format clock:
  fc.lngfc   = -1;
  fc.strname = "";

  // Category
  cat.cat     = SCAT_UNKNOWN;
  cat.lngcat  = -1;
  cat.strname = "";

  // Alternative category
  alt_cat.cat     = SCAT_UNKNOWN;
  alt_cat.lngcat  = -1;
  alt_cat.strname = "";

  // Sub-category
  sub_cat.strsub_cat = "";
  sub_cat.strname    = "";
  sub_cat.strdir     = "";

  // Alternative sub-category:
  alt_sub_cat.strsub_cat = "";
  alt_sub_cat.strname    = "";
  alt_sub_cat.strdir     = "";

  // Segment-specific info
  lngfc_seg           = -1;             // Reference to a tlkfc_seg record;
  sequence            = SSEQ_UNKNOWN;   // Order to play items in.
  string strspecific_media = "";        // Media to play if the user chose Specific
  blnpromos           = false;          // Promos allowed in this segment?
  blnmusic_bed        = false;          // Does this segment have a music bed?

  // Information about the music bed
  music_bed.strsub_cat = "";
  music_bed.strname    = "";
  music_bed.strdir     = "";

  blncrossfading = false; // Crossfade music & announcements in this segment?
  blnmax_age   = false;   // Does this segment limit the maximum age of sub-category media played?
  intmax_age   = -1;      // If so, this is the maximum age.
  blnpremature = false;   // Ignore the "Relevant from" setting of sub-category media
  blnrepeat    = false;   // Repeat sub-category media in this segment?

  // Current state (playing from category, alternate category, or default music profile)
  playback_state = PBS_DEFAULT_MUSIC;

  // Scheduled start and end time:
  scheduled.dtmstart = now();
  scheduled.dtmend   = now() + 60*60;

  // Playback timing:
  intlength = INT_MAX; // Length that this segment is meant to play for. Using a high default limit instead of 0.
  dtmstart = datetime_error; // Gets set when the first item from the segment is about to start playing.

  // Information used to retrieve the "next" item:
  programming_elements.clear();
  next_item = programming_elements.begin();
  blnfirst_fetched = false;
}

void segment::load_from_db(pg_connection & db, const long lngfc_seg_arg, const string & strdefault_music_source, const datetime dtmtime) {
  // Read details for a segement [lngfc_seg_arg], from the database(db), into this object  
  // Is a -1 lngfc_seg_arg specified? (ie, load default music profile)
  lngfc_seg = lngfc_seg_arg;
  if (lngfc_seg == -1) {
    log_message("Setting up default music profile...");
    setup_as_music_profile(strdefault_music_source, "<Default Music Profile>", db);
    playback_state = PBS_DEFAULT_MUSIC;
    blnloaded = true;

    // Fill in a bogus scheduled from and to:
    scheduled.dtmstart = dtmtime;
    scheduled.dtmend = dtmtime + 60*60 - 1;
    
    return; // Now return to the caller
  }

  // Otherwise, load details from the database:

  // Build main query:
  string strsql =
    "SELECT "
      "lngfc,"
      "tblfc.strname AS strfc_name,"
      "tblfc_seg.lngcat,"
      "(SELECT strname FROM tlkfc_cat WHERE lngfc_cat = tblfc_seg.lngcat) AS strcat_name,"
      "tblfc_seg.lngalt_cat,"
      "(SELECT strname FROM tlkfc_cat WHERE lngfc_cat = tblfc_seg.lngalt_cat) AS stralt_cat_name,"
      "tblfc_seg.strsub_cat,"
      "tblfc_seg.stralt_sub_cat,"
      "tblfc_seg.lngfc_seg,"
      "tlkfc_seq.strname as strseq,"
      "tblfc_media.strfile AS strspecific_media,"
      "(SELECT strdir FROM tlkfc_sub_cat WHERE lngfc_sub_cat = tblfc_media.lngsub_cat) AS strspecific_media_dir,"
      "tblfc_seg.dtmstart,"
      "tblfc_seg.dtmend,"
      "tblfc_seg.ysnpromos,"
      "tblfc_seg.ysnmusic_bed,"
      "tblfc_seg.lngmusic_bed_sub_cat,"
      "(SELECT strname FROM tlkfc_sub_cat WHERE lngfc_sub_cat = tblfc_seg.lngmusic_bed_sub_cat) AS strmusic_bed_name,"
      "(SELECT strdir  FROM tlkfc_sub_cat WHERE lngfc_sub_cat = tblfc_seg.lngmusic_bed_sub_cat) AS strmusic_bed_dir,"
      "tblfc_seg.ysncrossfade,"
      "tblfc_seg.intmax_age,"
      "tblfc_seg.ysnpremature,"
      "tblfc_seg.ysnrepeat "
    "FROM tblfc_seg "
    "INNER JOIN tblfc USING (lngfc) "
    "INNER JOIN tlkfc_seq ON tblfc_seg.lngseq = tlkfc_seq.lngfc_seq "
    "LEFT OUTER JOIN tblfc_media ON tblfc_seg.lngspecific_seq_media = tblfc_media.lngfc_media "
    "WHERE lngfc_seg = " + ltostr(lngfc_seg);

  // Fetch results
  pg_result rs = db.exec(strsql);

  // Check rowcount:
  if (rs.recordcount() != 1) my_throw("Error! " + itostr(rs.recordcount()) + " results were returned here instead of 1!");

  // Now load object fields of the resultset:

  // Information about the format clock:
  fc.lngfc   = strtol(rs.field("lngfc"));
  fc.strname = rs.field("strfc_name");

  // Category
  cat.lngcat  = strtol(rs.field("lngcat"));
  cat.strname = rs.field("strcat_name");
  cat.cat     = parse_category_string(cat.strname);
  
  // Sub-category
  load_sub_cat_struct(sub_cat, rs.field("strsub_cat"), db, cat, lngfc_seg, "Sub-category", "strsub_cat");

  // Alternative category
  alt_cat.lngcat  = strtol(rs.field("lngalt_cat", "-1"));
  alt_cat.strname = rs.field("stralt_cat_name", "");
  if (alt_cat.strname == "") {
    // Alternative category wasn't defined
    alt_cat.cat = SCAT_UNKNOWN;
  }
  else {
    testing_throw;
    // Alternative category was defined.
    alt_cat.cat = parse_category_string(alt_cat.strname);    

    // Alternative sub-category:
    load_sub_cat_struct(alt_sub_cat, rs.field("stralt_sub_cat"), db, alt_cat, lngfc_seg, "Alternative sub-category", "stralt_sub_cat");
  }

  // Segment-specific info
  sequence            = parse_sequence_string(rs.field("strseq"));  // Random, Sequential, Specific
  strspecific_media   = ensure_last_char(rs.field("strspecific_media_dir", ""), '/') + rs.field("strspecific_media", "");        // Media to play if the user chose Specific

  blnpromos           = strtobool(rs.field("ysnpromos"));          // Promos allowed in this segment?
  blnmusic_bed        = strtobool(rs.field("ysnmusic_bed"));       // Does this segment have a music bed?

  // Information about the music bed.
  music_bed.strsub_cat = rs.field("lngmusic_bed_sub_cat", "-1");
  music_bed.strname    = rs.field("strmusic_bed_name", "");
  music_bed.strdir     = rs.field("strmusic_bed_dir", "");

  blncrossfading = strtobool(rs.field("ysncrossfade")); // Crossfade music & announcements in this segment?
  blnmax_age   = !(rs.field_is_null("intmax_age"));   // Does this segment limit the maximum age of sub-category media played?
  intmax_age   = strtoi(rs.field("intmax_age", "-1"));      // If so, this is the maximum age.
  blnpremature = strtobool(rs.field("ysnpremature"));   // Ignore the "Relevant from" setting of sub-category media
  blnrepeat    = strtobool(rs.field("ysnrepeat"));   // Repeat sub-category media in this segment?

  // Try to load the list of programming elements:
  playback_state = PBS_CATEGORY; // If this fails, we go to alternate segment, default music, etc.
  {
    bool blnsuccess = false; // Set to true if we successfully load the list:
    try {
      load_pe_list(programming_elements, cat, sub_cat, db);
      next_item = programming_elements.begin();
      blnsuccess = true; // The above succeeded.
    } catch_exceptions;
    // If this fails, we revert to a lower level:
    if (!blnsuccess) {
      revert_down(db, strdefault_music_source);
    }
  }

  // Calculate scheduled.dtmstart and scheduled.dtmend  (full date & time, not just minute and second).
  {
    datetime dtmstart = parse_time_string(rs.field("dtmstart"));
    datetime dtmend   = parse_time_string(rs.field("dtmend"));

    // Fetch the full version of "dtmstart"
    {
      tm tmall  = datetime_to_tm(dtmtime);
      tm tmmmss = datetime_to_tm(dtmstart);
      tmall.tm_min = tmmmss.tm_min;
      tmall.tm_sec = tmmmss.tm_sec;
      scheduled.dtmstart = mktime(&tmall);
    }

    // Fetch the full version of "dtmend"
    {
      tm tmall  = datetime_to_tm(dtmtime);
      tm tmmmss = datetime_to_tm(dtmend);
      tmall.tm_min = tmmmss.tm_min;
      tmall.tm_sec = tmmmss.tm_sec;
      scheduled.dtmend = mktime(&tmall);
    }
  }
  
  // Everything was successfully loaded:
  blnloaded = true;

  // The variable "dtmstarted" gets set later, when the first item from this segment actually starts playing.
  // This is used to help ensure that the segment does in fact play it's full length.
}

void segment::setup_as_music_profile(const string & strmusic_source, const string & strdesc, pg_connection & db) {
  // Segment-specific info
  cat.cat     = SCAT_MUSIC;
  cat.strname = "Music";
  
  sequence       = SSEQ_RANDOM; // Play music in random order
  blnpromos      = true; // Allow announcement playback during this segment.
  blnrepeat      = true; // Music
  blncrossfading = true; // Music items crossfade into each other.

  // Now generate a music playlist from the specified location:
  generate_playlist(programming_elements, strmusic_source, db); // Also shuffles the list

  // Shuffle it:
  shuffle_pel(programming_elements);

  // And setup the variables used to track which is the next item to be returned...
  next_item    = programming_elements.begin(); // The next item to be returned when requested...

  // And at the end:
  blnloaded = true; // Our segment is now loaded.
}   

void segment::get_next_item(programming_element & pe, pg_connection & db, const string & strdefault_music_source, const int intstarts_ms) {
  // Has the first item already been retrieved?
  if (blnfirst_fetched) {
    // First item has already been fetched. Go to the next item
    // (ie, we don't progress to the next item until the 2nd item is being retrieved).

    // Are we at the end of the current list?
    if (next_item == programming_elements.end()) my_throw("Logic Error!"); // Should never be at end before advancing!
    next_item++; // Go to the next item.

    // At the last item now?
    if (next_item == programming_elements.end()) {
      // Yes. Does the segment allow repeating?
      if (blnrepeat) {
        // Yes. Go back to the beginning of the list.
        log_message("Ran out of media, going back to the beginning of the playlist");
        next_item = programming_elements.begin();
      }
      else {
        // Repeating not allowed. Revert to the alternate category.
        // "imaging filler", or we revert to the alternate category.
        log_line("Ran out of media for this segment (repeat=false)");
        revert_down(db, strdefault_music_source); // This will also setup "next_item"
      }
    }
  }

  // Check: Do we have a "next" item to return?
  if (next_item == programming_elements.end()) my_throw("Logic Error!"); // This should never happen...
  
  // Now return it:
  pe = *next_item;

  // And now we've definitely returned the "first" item from the list if we hadn't
  // already:
  blnfirst_fetched = true; // Next time we will advance to the next item.
}

void segment::generate_playlist(programming_element_list & pel, const string & strsource, pg_connection & db) {
  // Process a directory or M3U file and generate a list of media to play during this segment.
  pel.clear(); // Clear anything already in the program element list.

  // Call a recursive function to load directories, m3us, etc. Directories can contain M3U files
  // And M3U files can list directories. Go down to a maximum of 3 levels of recursion. Also, if
  // we encounter a directory, we check if it is one of the format clock-subdirectories. If it is, then
  // apply special logic to see which files to actually use (relevant from, until, etc)
  vector <string> file_list;

  recursive_add_to_string_list(file_list, strsource, 3, db);

  // Sort the entries:
  sort (file_list.begin(), file_list.end());

  // Remove the duplicate entries:
  {
    vector <string>::iterator i=file_list.begin();
    string strlast = "";
    while (i != file_list.end()) {
      // Is this entry the same as the previous entry?
      if ((*i)== strlast) {
        testing_throw;
        // Yes - erase it.
        i = file_list.erase(i);
      }
      else {
        // No: It is our new "last" line:
        strlast = *i;
        ++i;
      }
    }
  }

  // Check for LineIn. Can't be mixed with MP3s, etc
  {
    int intnon_linein=0; // Count how many non-linein entries we find
    int intlinein=0;     // Count how many linein entries we find.

    vector <string>::iterator i=file_list.begin();
    while (i != file_list.end()) {
      if (*i == "LineIn") {
        testing_throw;
        ++intlinein;
      }
      else {
        ++intnon_linein;
      }
      ++i;
    }

    if (intlinein > 0) {
      testing_throw;
      // Yes. Were there non-linein entries?
      if (intnon_linein > 0) {
        testing_throw;
        // Yes. This music source has both linein and non-linein entries
        my_throw("Invalid music source! It has both LineIn and non-LineIn entries: " + strsource);
      }
      // Did we find more than one line-in entry?
      if (intlinein > 1) {
        testing_throw;
        log_warning("Music source lists # LineIn entries. This is weird, please look into this.");
        // Reset the list, push just 1 "LineIn" entry into it:
        file_list.clear();
        file_list.push_back("LineIn");        
      }
      testing_throw;
    }
  }

  // Now populate the programming element list, and add a music bed if appropriate:
  {
    vector <string>::iterator i=file_list.begin();
    while (i != file_list.end()) {
      // Create a new programming element:
      programming_element pe;
      // Basic details:
      pe.cat = cat.cat;
      pe.strmedia = *i;
      pe.strvol = (pe.cat == SCAT_MUSIC ? "MUSIC" : "PROMO");
      // Add a Music bed?
      if (blnmusic_bed) {
        // Yes. Set music bed details:
        pe.music_bed.strmedia    = get_music_bed_media(strtol(music_bed.strsub_cat), db);
        pe.music_bed.strvol      = "MUSIC";
        pe.music_bed.intstart_ms = 0; // Not yet using this functionality.
        pe.music_bed.intlength_ms = 60*60; // Not yet using this functionality.
        pe.blnmusic_bed = true;
      }
      pe.blnloaded = true;
      pel.push_back(pe);
      ++i;
    }
  }

  // Throw an exception if nothing was returned:
  if (pel.size() <= 0) {
    // Throw an exception if there are no entries:
    my_throw("Could not find any media for the playlist: \"" + strsource + "\"");
  }
  else if (cat.cat == SCAT_MUSIC && pel.size() < 10) {
    // Log a warning if this is a music segment and there are very few entries:    
    log_warning("The new music playlist only has " + itostr(pel.size()) + " song(s)!");    
  }
}

void segment::shuffle_pel(programming_element_list & pel) {
  // Shuffle a programming element list:
  srand(now());
  // Make 10 passes through the list
  programming_element pe;
  for(unsigned intouter=0; intouter<=9; intouter++) {
    for(unsigned intinner=0; intinner<pel.size(); intinner++) {
      int intrand_pos = rand() % pel.size();

      // Swap the two Programing elements;
      pe = pel[intinner];
      pel[intinner] = pel[intrand_pos];
      pel[intrand_pos] = pe;
    }
  }
}

// Function called by load_from_db: Prepare a list of programming elements to use, based on the segment parameters.
void segment::load_pe_list(programming_element_list & pel, const struct cat & cat, const struct sub_cat & sub_cat, pg_connection & db) {
  // Clear out the current program element list:
  pel.clear();
  bool blnshuffle_pel = false; // Set to true if we are shuffle pel at the end of the function

  // Check the CATEGORY TYPE:
  switch (cat.cat) {
    // We don't load any media for SILENCE segments:
    case SCAT_SILENCE: return; break;

    // The valid categories (where stuff gets loaded):
    case SCAT_IMAGING: case SCAT_MUSIC: case SCAT_NEWS: case SCAT_SWEEPERS:
    case SCAT_LINKS: case SCAT_ENTERTAINMENT: case SCAT_PROMOS: break;

    // If for some strange reason there is a music category loaded:
    case SCAT_MUSIC_BED: log_warning("This is a Music Bed segment!");

    // All other categories: Invalid! They aren't meant to be used for the segment!
    default: my_throw("Logic Error!");
  }

  string strsource = ""; // The file, sub-directory or playlist we use:
  
  // Is the sequence "Specific"? (ie, use only 1 media item)
  if (sequence == SSEQ_SPECIFIC) {
testing_throw;
    // Prepare a single programming element and add it to the list.
    if (!file_exists(strspecific_media)) my_throw("Segment's 'specific' media not found: " + strspecific_media);
    strsource = strspecific_media;
  }
  else { // Random or Sequence
    // Prepare a list of all the (valid) media from under the sub-category.

    // Is strsub_cat numeric? (if so, it points to a tlkfc_sub_cat record)
    if (sub_cat.strsub_cat == "") my_throw("strsub_cat is not set!");

    if (isint(sub_cat.strsub_cat)) {
testing_throw;
      // strsub_cat is numeric. Fetch the sub-category's sub-directory.
      string strsql = "SELECT strdir FROM tlkfc_sub_cat WHERE lngfc_sub_cat = " + sub_cat.strsub_cat;
      pg_result rs = db.exec(strsql);
      if (rs.recordcount() == 0) my_throw("This segment lists it's sub-category (lngfc_sub_cat) as " + sub_cat.strsub_cat + ", but I could not find any matching tlkfc_sub_cat records.");
      strsource = rs.field("strdir");
      if (!dir_exists(strsource)) my_throw("The sub-category directory's is missing: " + strsource);
    }
    else {
      // strsub_cat is non-numeric. Use it directly as our "source" to build a playlist from:
      strsource = sub_cat.strsub_cat;
    }
  
    // Do we shuffle the media?
    blnshuffle_pel = (sequence == SSEQ_RANDOM);
  }

  // Build up our list of items to play:
  generate_playlist(pel, strsource, db);

  // Did we get any files? (maybe they're all missing):
  if (pel.size() == 0) my_throw("I couldn't find anything to play!");

  // Shuffle the list if appropriate:
  if (blnshuffle_pel) {
    shuffle_pel(pel);
  }
}

void segment::revert_down(pg_connection & db, const string & strdefault_music_source) {
  // If there is a problem with playing category items, we revert to alternate category. If there is also a problem
  // with the alternate category, we attempt to revert to the default music profile. If there are still problems
  // we throw an exception. This function is called to revert from the current playback status to the next lower.
  bool blndone = false; // Set to true when we find the state to revert to.
  
  while (!blndone) {
    // Reset pe list, and current element pointer.
    programming_elements.clear();
    next_item = programming_elements.begin();
  
    switch (playback_state) {
      case PBS_CATEGORY: {
        log_message("Reverting to Alternative Category & Sub-Category");
        playback_state = PBS_ALTERNATE;
        try {
          // Check if the alternate category was defined:
          if (alt_cat.cat == SCAT_UNKNOWN) my_throw("Alternate Category was not defined.");
          load_pe_list(programming_elements, alt_cat, alt_sub_cat, db);
          next_item = programming_elements.begin();
          blndone = true;
        } catch_exceptions;
      } break;
      case PBS_ALTERNATE: {
        log_message("Reverting to Default Music Profile");
        playback_state = PBS_DEFAULT_MUSIC;
        try {
          setup_as_music_profile(strdefault_music_source, "<Default Music Profile>", db);
          next_item = programming_elements.begin();
          // Also allow promos to play now:
          blnpromos = true;
          blndone = true;
        } catch_exceptions;
      } break;
      case PBS_DEFAULT_MUSIC: {
        testing;
        my_throw("There was a problem with the Default music profile, but there is nothing else to play!");
        blndone = true;
      } break;
      default: my_throw("Logic Error!");
    }
  }
}

// Functions called by load_from_db:
seg_category segment::parse_category_string(const string & strcat) {
  string str = lcase(trim(strcat));
  if (str == "imaging")          return SCAT_IMAGING;
  if (str == "music")            return SCAT_MUSIC;
  if (str == "news")             return SCAT_NEWS;
  if (str == "sweepers")         return SCAT_SWEEPERS;
  if (str == "links")            return SCAT_LINKS;
  if (str == "entertainment")    return SCAT_ENTERTAINMENT;
  if (str == "promos")           return SCAT_PROMOS;
  if (str == "music bed")        return SCAT_MUSIC_BED;
  if (str == "silence")          return SCAT_SILENCE;

  my_throw("Unknown Segment Category: \"" + strcat + "\"");
}

segment::seg_sequence segment::parse_sequence_string(const string & strseq) {
  string str = lcase(trim(strseq));
  if (str == "random")   return SSEQ_RANDOM;
  if (str == "sequential") return SSEQ_SEQUENTIAL;
  if (str == "specific") return SSEQ_SPECIFIC;
  my_throw("Unknown Sequence Category: \"" + strseq + "\"");
}

void segment::load_sub_cat_struct(struct sub_cat & sub_cat, const string strsub_cat, pg_connection & db, const struct cat & cat, const long lngfc_seg, const string & strdescr, const string & strfield) {
  // Load sub-category details from the database.
  sub_cat.strsub_cat = strsub_cat; // Load from the arg into the struct

  // sub-category in integer form?
  if (isint(strsub_cat)) {
  testing_throw;
    // strsub_cat points to a tlkfc_sub_cat record

    // Fetch category details:
    string strsql = "SELECT strname, strdir FROM tlkfc_sub_cat WHERE lngfc_sub_cat = " + strsub_cat + " AND lngfc_cat = " + ltostr(cat.lngcat);
    pg_result rs = db.exec(strsql);

    // Check # of returned rows:
    if (rs.recordcount() != 1) my_throw("Found " + itostr(rs.recordcount()) + " " + strdescr + " records for segment " + ltostr(lngfc_seg) + ", expected 1!");

    // Fetch the category name and directory:
    sub_cat.strname = rs.field("strname");
    sub_cat.strdir  = rs.field("strdir");
  }
  else {
    // strsub_cat lists a sub-directory or m3u file:
    if (!file_exists(strsub_cat) && !dir_exists(strsub_cat)) my_throw("Could not find a file or directory called '" + strsub_cat + "'");
    sub_cat.strname = strsub_cat;
  }
}

string segment::get_music_bed_media(const long lngsub_cat, pg_connection & db) {
  // Fetch an underlying music media file of the specified sub-category (random):
  string strsql = "SELECT strfile, strdir FROM tblfc_media INNER JOIN tlkfc_sub_cat ON tblfc_media.lngsub_cat = tlkfc_sub_cat.lngfc_sub_cat WHERE lngsub_cat = " + ltostr(lngsub_cat);
  pg_result rs = db.exec(strsql);
  if (rs.recordcount() == 0) my_throw("Could not find music bed media in the database (lngsub_cat=" + itostr(lngsub_cat) +")!");

  // Chooe a random file from the recordset:
  int intrand = rand() % rs.recordcount();

  for (int i = 0; i < intrand; i++) rs.movenext();
  string strfile = ensure_last_char(rs.field("strdir"), '/') + rs.field("strfile");

  // Check if the media exists:
  if (!file_exists(strfile)) my_throw("Music bed media listed in database but not found on disk: " + strfile);

  return strfile;
}


// A recursive function used to load m3u files that contain directories, and directories which contain m3us:
// Also applies special logic to format clock sub-category directories
void segment::recursive_add_to_string_list(vector <string> & file_list, const string & strsource, const int intrecursion_level, pg_connection & db) {
  // Call a recursive function to load directories, m3us, etc. Directories can contain M3U files
  // And M3U files can list directories. Go down to a maximum of 3 levels of recursion. Also, if
  // we encounter a directory, we check if it is one of the format clock-subdirectories. If it is, then
  // apply special logic to see which files to actually use (relevant from, until, etc)

  // Bomb out if our recursion level is too low (ie, this function was called incorrectly)
  if (intrecursion_level < 0) my_throw("Logic Error!");

  // LineIn?
  if (strsource == "LineIn") {
    testing_throw;
    file_list.push_back(strsource);
    return; // Done handling the source.
  }

  // A directory?
  if (dir_exists(strsource)) {
    // One of the format clock sub-category directories?
    string strdir = ensure_last_char(strsource, '/');
    string strsql = "SELECT lngfc_sub_cat FROM tlkfc_sub_cat WHERE strdir = " + psql_str(strdir);
    pg_result rs = db.exec(strsql);
    if (rs.recordcount() > 0) {
      long lngfc_sub_cat = strtol(rs.field("lngfc_sub_cat"));
      
      // A format clock sub-category directory. Fetch relevant MP3s from the database:
      string strsql_date = "'" + format_datetime(date(), "%F") + "'"; // Current date, in psql form.
      string strsql = "SELECT strfile FROM tblfc_media WHERE "
                      "lngcat = " + ltostr(cat.lngcat) + " AND "
                      "lngsub_cat = " + ltostr(lngfc_sub_cat) + " AND "
                      "COALESCE(dtmrelevant_until, '9999-12-25') >= " + strsql_date;
      // Now modify the query, using the Max Age and Premature segment settings.
      if (!blnpremature) { // blnpremature means ignore relevant from
        strsql += " AND COALESCE(dtmrelevant_from, '0000-01-01') <= " + strsql_date;
      }

      if (blnmax_age) { // Max age means maximum # of days after dtmrelevant_from that the media will be played.
        testing_throw;
        strsql += " AND COALESCE(dtmrelevant_from, '0000-01-01') + " + itostr(intmax_age - 1) + " >= " + strsql_date;
      }

      // Order the media by filename:
      strsql += " ORDER BY strfile";
      pg_result rs = db.exec(strsql);

      // Did we get anything?
      if (rs.recordcount() == 0) {
        // No:
        log_warning("Database does not list any usable format clock sub-category media under this directory: " + strdir);
        return;
      }

      // Process records:
      {
        int intadded=0; // Number if items we've added to the file list
        while (!rs.eof()) {
          // Fetch the file from the database:
          string strfile = rs.field("strfile", "");
          // Exists on the harddrive?
          if (file_exists(strdir + strfile)) {
            // Yes. Add it.
            file_list.push_back(strdir + strfile);
            ++intadded;
          }
          else {
            testing;
            // No. Log a warning:
            log_warning("File listed in the database, but not found on disk: " + strdir + strfile);
          }
          rs.movenext();
        }
        // Did we add any entries?
        if (intadded <= 0) {
          testing_throw;
          // No. Log a warning.
          log_warning("Could not find any usable format clock sub-category media under this directory: " + strdir);
          testing_throw;
        }
      }
    }
    else {
      // Not a format clock sub-category directory. Process all the files.
      int intadded = 0; // Number of files we used under this directory

      // Load all MP3 files into the list
      {
        dir_list dir(strdir, ".mp3", DT_REG | DT_LNK);
        string strfile = dir;
        while (strfile != "") {
          file_list.push_back(strdir + strfile);
          ++intadded;
          strfile = dir;
        }
      }

      // Load all M3U files into the list (but only if our recursion level is high enough)
      dir_list dir(strdir, ".m3u", DT_REG | DT_LNK);
      string strfile = dir;
      while (strfile != "") {
        if (intrecursion_level > 0) {
          // Process contents of the M3U file:
          recursive_add_to_string_list(file_list, strdir + strfile, intrecursion_level - 1, db);
          ++intadded;
        }
        else {
          testing;
          // Can't process the M3U file, have reached recursion level.
          log_warning("Not processing M3U file " + strdir + strfile + ". I am already at my maxiumum search depth.");
          testing;
        }
        strfile = dir;
      }

      // Log a warning if we didn't find any files:
      if (intadded == 0) {
        testing_throw;
        log_warning("Didn't find any usable files under this directory: " + strdir);
        testing_throw;
      }
    }
    return; // Done with directory source handling.
  }

  // A file?
  if (file_exists(strsource)) {
    // What is the file extension?
    string strext=lcase(right(strsource, 4));
    if (strext==".mp3") {
      testing_throw;
      // A MP3 file. Add it to the list:
      file_list.push_back(strsource);
      testing_throw;
    }
    else if (strext==".m3u") {
      // A M3U file. Are we at our recursion level?
      if (intrecursion_level <= 0) {
        testing_throw;
        // Yes. We can't process it.
        log_warning("Not processing M3U file " + strsource + ". I am already at my maxiumum search depth.");
        testing_throw;
      }
      else {
        // No. Attempt to open and read lines from the file:
        ifstream m3u_file(strsource.c_str());
        if (!m3u_file) {
          testing_throw;
          log_warning("Unable to open M3U file: " + strsource);
          testing_throw;
        }
        else {
          // Process all lines in the M3U file:
          int intadded = 0; // Lines we've used from the file:
          string strline="";
          while (getline(m3u_file, strline)) {
            // Skip empty lines and lines beginning with #:
            if (strline != "" && strline[0] != '#') {
              // Line should be fine. Process the line:
              recursive_add_to_string_list(file_list, strline, intrecursion_level - 1, db);
              ++intadded;
            }
          }
          // Did we get any usable lines?
          if (intadded <= 0) {
            testing_throw;
            log_warning("Didn't find any usable lines in M3U file: " + strsource);
            testing_throw;
          }
        }
      }
    }
    else {
      testing_throw;
      // Invalid file extension. Log a warning
      log_warning("I don't recognise the '" + strext + "' extension on this file: " + strsource);
      testing_throw;
    }
    return; // Done with the file source handling.
  }

  // Could not find the source:
  testing;
  log_warning("Source not found: \"" + strsource + "\"");
  testing;
}
