
#include "segment.h"
#include "player_util.h"
#include "common/exception.h"
#include "common/file.h"
#include "common/dir_list.h"
#include "common/my_string.h"
#include "common/rr_misc.h"
#include <fstream>
#include <linux/cdrom.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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
  intmax_items = INT_MAX; // Maximum number of items allowed to play during this segment;

  // Current state (playing from category, alternate category, or the current music profile)
  playback_state = PBS_MUSIC_PROFILE;

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
  intnum_fetched = 0;
}

void segment::load_from_db(pg_connection & db, const long lngfc_seg_arg, const datetime dtmtime, const player_config & config) {
  // Read details for a segment [lngfc_seg_arg], from the database(db), into this object
  // Is a -1 lngfc_seg_arg specified? (ie, attempt to load current music profile)

  // First reset all existing info & stats:
  reset();

  // Now start the loading:
  lngfc_seg = lngfc_seg_arg;
  try {
    if (lngfc_seg == -1) {
      log_message("Setting up music profile...");
      load_music_profile(db, config);
      playback_state = PBS_MUSIC_PROFILE;
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
        "COALESCE (tblfc_seg.ysnpromos, (SELECT blndefault_promos FROM tlkfc_cat WHERE lngfc_cat = tblfc_seg.lngcat)) AS ysnpromos,"
        "tblfc_seg.ysnmusic_bed,"
        "tblfc_seg.lngmusic_bed_sub_cat,"
        "(SELECT strname FROM tlkfc_sub_cat WHERE lngfc_sub_cat = tblfc_seg.lngmusic_bed_sub_cat) AS strmusic_bed_name,"
        "(SELECT strdir  FROM tlkfc_sub_cat WHERE lngfc_sub_cat = tblfc_seg.lngmusic_bed_sub_cat) AS strmusic_bed_dir,"
        "COALESCE (tblfc_seg.ysncrossfade, (SELECT blndefault_crossfade FROM tlkfc_cat WHERE lngfc_cat = tblfc_seg.lngcat)) AS ysncrossfade,"
        "tblfc_seg.intmax_age,"
        "tblfc_seg.ysnpremature,"
        "COALESCE (tblfc_seg.ysnrepeat, (SELECT blndefault_repeat FROM tlkfc_cat WHERE lngfc_cat = tblfc_seg.lngcat)) AS ysnrepeat,"
        "tblfc_seg.intmax_items "
      "FROM tblfc_seg "
      "INNER JOIN tblfc USING (lngfc) "
      "INNER JOIN tlkfc_seq ON tblfc_seg.lngseq = tlkfc_seq.lngfc_seq "
      "LEFT OUTER JOIN tblfc_media ON tblfc_seg.lngspecific_seq_media = tblfc_media.lngfc_media "
      "WHERE lngfc_seg = " + ltostr(lngfc_seg);

    // Fetch results
    pg_result rs = db.exec(strsql);

    // Check rowcount:
    if (rs.size() != 1) my_throw("Error! " + itostr(rs.size()) + " results were returned here instead of 1!");

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
      // Alternative category was defined.
      alt_cat.cat = parse_category_string(alt_cat.strname);

      // Alternative sub-category:
      // Do we have an alternative sub-category?
      string stralt_sub_cat=rs.field("stralt_sub_cat", "");
      if (stralt_sub_cat == "") my_throw("Alternative category defined but not the alternative sub-category!");

      load_sub_cat_struct(alt_sub_cat, rs.field("stralt_sub_cat", ""), db, alt_cat, lngfc_seg, "Alternative sub-category", "stralt_sub_cat");
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

    // Don't allow music beds to play with Music segments:
    if (blnmusic_bed && cat.cat == SCAT_MUSIC) my_throw("Music segments aren't allowed to have music beds!");

    blncrossfading = strtobool(rs.field("ysncrossfade")); // Crossfade music & announcements in this segment?
    blnmax_age   = !(rs.field_is_null("intmax_age"));   // Does this segment limit the maximum age of sub-category media played?
    intmax_age   = strtoi(rs.field("intmax_age", "-1"));      // If so, this is the maximum age.
    blnpremature = strtobool(rs.field("ysnpremature"));   // Ignore the "Relevant from" setting of sub-category media
    blnrepeat    = strtobool(rs.field("ysnrepeat"));   // Repeat sub-category media in this segment?
    intmax_items = strtoi(rs.field("intmax_items", itostr(INT_MAX).c_str()));

    // Try to load the list of programming elements:
    playback_state = PBS_CATEGORY; // If this fails, we go to alternate segment, default music, etc.
    {
      bool blnsuccess = false; // Set to true if we successfully load the list:
      try {
        load_pe_list(programming_elements, cat, sub_cat, db, config);
        next_item = programming_elements.begin();
        blnsuccess = true; // The above succeeded.
      } catch_exceptions;
      // If this fails, we revert to a lower level:
      if (!blnsuccess) {
        revert_down(db, config);
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
  }
  catch (const exception & e) {
    // There was a problem loading the segment. Try reverting to the current music profile instead.
    // Were already processing a music profile?
    if (lngfc_seg == -1) throw;

    // Otherwise, attempt to revert to default music:
    log_error((string)"The following error occured while loading the segment details: " + e.what());
    log_message("Reverting to a music profile.");

    load_from_db(db, -1, dtmtime, config);
    lngfc_seg = lngfc_seg_arg; // And restore the value. We are playing default music, but our segment remains unchanged.
  }

  // The variable "dtmstarted" gets set later, when the first item from this segment actually starts playing.
  // This is used to help ensure that the segment does in fact play it's full length.
}

void segment::load_music_profile(pg_connection & db, const player_config & config) {
  // Segment-specific info
  cat.cat     = SCAT_MUSIC;
  cat.strname = "Music";

  sequence       = SSEQ_RANDOM; // Play music in random order
  blnpromos      = true; // Allow announcement playback during this segment.
  blnrepeat      = true; // Music
  blncrossfading = true; // Music items crossfade into each other.
  blnmusic_bed   = false; // Music profiles don't have underlying music.

  // Load the current music profile into the list of programming elements:
  generate_playlist(programming_elements, "MusicProfile", SCAT_MUSIC, db, config); // Also shuffles the list

  // Shuffle it:
  shuffle_pel(programming_elements);

  // And setup the variables used to track which is the next item to be returned...
  next_item = programming_elements.begin(); // The next item to be returned when requested...

  // And at the end:
  blnloaded = true; // Our segment is now loaded.
}

void segment::get_next_item(programming_element & pe, pg_connection & db, const int intstarts_ms, const player_config & config) {
  // Have we already fetched the maximum allowed number of items for this segment?
  if (intnum_fetched >= intmax_items) {
    // We've feched the maximum number of allowed items. Tell the user & revert down.
    log_message("Have already played the maximum allowed number of items for this segment (" + itostr(intmax_items) + ")");
    revert_down(db, config); // This will also setup "next_item"
  }
  else {
    // Has the first item already been retrieved?
    if (blnfirst_fetched) {
      // First item has already been fetched. Go to the next item
      // (ie, we don't progress to the next item until the 2nd item is being retrieved).

      // Are we at the end of the current list?
      if (next_item == programming_elements.end()) LOGIC_ERROR; // Should never be at end before advancing!
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
          revert_down(db, config); // This will also setup "next_item"
        }
      }
    }
  }

  // Check: Do we have a "next" item to return?
  if (next_item == programming_elements.end()) LOGIC_ERROR; // This should never happen...

  // Now return it:
  pe = *next_item;

  // And now we've definitely returned the "first" item from the list if we hadn't
  // already:
  blnfirst_fetched = true; // Next time we will advance to the next item.
  ++intnum_fetched;
}

void segment::generate_playlist(programming_element_list & pel, const string & strsource, const seg_category pel_cat, pg_connection & db, const player_config & config) {
  // Process a directory or M3U file and generate a list of media to play during this segment.
  pel.clear(); // Clear anything already in the program element list.

  // Call a recursive function to load directories, m3us, etc. Directories can contain M3U files
  // And M3U files can list directories. Go down to a maximum of 3 levels of recursion. Also, if
  // we encounter a directory, we check if it is one of the format clock-subdirectories. If it is, then
  // apply special logic to see which files to actually use (relevant from, until, etc)
  vector <string> file_list;

  recursive_add_to_string_list(file_list, strsource, 3, db, config);

  // Sort the entries:
  sort (file_list.begin(), file_list.end());

  // Remove the duplicate entries:
  {
    vector <string>::iterator i=file_list.begin();
    string strlast = "";
    while (i != file_list.end()) {
      // Is this entry the same as the previous entry?
      if ((*i)== strlast) {
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
  bool blnlinein = false; // Set to true below if the playlist specifies LineIn
  {
    int intnon_linein=0; // Count how many non-linein entries we find
    int intlinein=0;     // Count how many linein entries we find.

    vector <string>::iterator i=file_list.begin();
    while (i != file_list.end()) {
      if (*i == "LineIn")
        ++intlinein;
      else
        ++intnon_linein;
      ++i;
    }

    if (intlinein > 0) {
      // Yes.
      blnlinein = true; // Used later in this function
      // Were there non-linein entries?
      if (intnon_linein > 0) {
        // Yes. This music source has both linein and non-linein entries
        my_throw("Invalid music source! It has both LineIn and non-LineIn entries: " + strsource);
      }
      // Did we find more than one line-in entry?
      if (intlinein > 1) {
        log_warning("Music source lists # LineIn entries. This is weird, please look into this.");
        // Reset the list, push just 1 "LineIn" entry into it:
        file_list.clear();
        file_list.push_back("LineIn");
      }
    }
  }

  // Now populate the programming element list, and add a music bed if appropriate:
  {
    vector <string>::iterator i=file_list.begin();
    while (i != file_list.end()) {
      // Create a new programming element:
      programming_element pe;
      // Basic details:
      pe.cat = pel_cat;
      pe.strmedia = *i;
      pe.strvol = (pel_cat == SCAT_MUSIC ? "MUSIC" : "PROMO");
      // Add a Music bed?
      if (blnmusic_bed) {
        // Yes. Set music bed details:
        pe.music_bed.strmedia    = get_music_bed_media();
        pe.music_bed.strvol      = "MUSIC";
        pe.music_bed.intstart_ms = 0; // Not yet using this functionality.
        pe.music_bed.intlength_ms = 1000*60*60; // Not yet using this functionality.
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
  else if (cat.cat == SCAT_MUSIC && !blnlinein && pel.size() < 10) {
    // Log a warning if this is a non-linein music segment and there are very few entries:
    log_warning("The new music playlist only has " + itostr(pel.size()) + " song(s)!");
  }
}

// Function passed to random_shuffle...
inline int myrand(const int & i) {
  return rand()%i;
}

void segment::shuffle_pel(programming_element_list & pel) {
  srand(now());
  random_shuffle(pel.begin(), pel.end(), myrand);
}

// Function called by load_from_db: Prepare a list of programming elements to use, based on the segment parameters.
void segment::load_pe_list(programming_element_list & pel, const struct cat & cat, const struct sub_cat & sub_cat, pg_connection & db, const player_config & config) {
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
    default: LOGIC_ERROR;
  }

  string strsource = ""; // The file, sub-directory or playlist we use:

  // Is the sequence "Specific"? (ie, use only 1 media item)
  if (sequence == SSEQ_SPECIFIC) {
    // Prepare a single programming element and add it to the list.
    if (!file_exists(strspecific_media)) my_throw("Segment's 'specific' media not found: " + strspecific_media);
    strsource = strspecific_media;
  }
  else { // Random or Sequence
    // Prepare a list of all the (valid) media from under the sub-category.

    // Is strsub_cat numeric? (if so, it points to a tlkfc_sub_cat record)
    if (sub_cat.strsub_cat == "") my_throw("strsub_cat is not set!");

    if (isint(sub_cat.strsub_cat)) {
      // strsub_cat is numeric. Fetch the sub-category's sub-directory.
      string strsql = "SELECT strdir FROM tlkfc_sub_cat WHERE lngfc_sub_cat = " + sub_cat.strsub_cat;
      pg_result rs = db.exec(strsql);
      if (rs.size() == 0) my_throw("This segment lists it's sub-category (lngfc_sub_cat) as " + sub_cat.strsub_cat + ", but I could not find any matching tlkfc_sub_cat records.");
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

  // List the music bed items to be used during this segment:
  if (blnmusic_bed) {
    list_music_bed_media(db);
  }

  // Build up our list of items to play:
  generate_playlist(pel, strsource, cat.cat, db, config);

  // Did we get any files? (maybe they're all missing):
  if (pel.size() == 0) my_throw("I couldn't find anything to play!");

  // Shuffle the list if appropriate:
  if (blnshuffle_pel) {
    shuffle_pel(pel);
  }
}

void segment::revert_down(pg_connection & db, const player_config & config) {
  // If there is a problem with playing category items, we revert to alternate category. If there is also a problem
  // with the alternate category, we attempt to revert to a music profile. If there are still problems
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
          // Disable some settings from the original category & sub-category:
          blnmusic_bed = false;
          sequence = SSEQ_RANDOM;
          intmax_items = INT_MAX;

          // If the alternate category is musical, then allow repetition:
          if (alt_cat.cat == SCAT_MUSIC) blnrepeat = true;

          // Check if the alternate category was defined:
          if (alt_cat.cat == SCAT_UNKNOWN) {
            log_warning("Alternate Category was not defined.");
          }
          else {
            // Now load the new playlist & setup the iterator:
            load_pe_list(programming_elements, alt_cat, alt_sub_cat, db, config);
            next_item = programming_elements.begin();
            blndone = true;
          }
        } catch_exceptions;
      } break;
      case PBS_ALTERNATE: {
        log_message("Reverting to a Music Profile");
        playback_state = PBS_MUSIC_PROFILE;
        try {
          load_music_profile(db, config); // Automatically reverts to default music if no profile can be found
          next_item = programming_elements.begin();
          blndone = true;
        } catch_exceptions;
      } break;
      case PBS_MUSIC_PROFILE: {
        my_throw("There was a problem with the Default music profile, but there is nothing else to play!");
        blndone = true;
      } break;
      default: LOGIC_ERROR;
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
  if (str == "random")     return SSEQ_RANDOM;
  if (str == "sequential") return SSEQ_SEQUENTIAL;
  if (str == "specific")   return SSEQ_SPECIFIC;
  my_throw("Unknown Sequence Category: \"" + strseq + "\"");
}

void segment::load_sub_cat_struct(struct sub_cat & sub_cat, const string strsub_cat, pg_connection & db, const struct cat & cat, const long lngfc_seg, const string & strdescr, const string & strfield) {
  // Load sub-category details from the database.
  sub_cat.strsub_cat = strsub_cat; // Load from the arg into the struct

  // sub-category in integer form?
  if (isint(strsub_cat)) {
    // strsub_cat points to a tlkfc_sub_cat record

    // Fetch category details:
    string strsql = "SELECT strname, strdir FROM tlkfc_sub_cat WHERE lngfc_sub_cat = " + strsub_cat + " AND lngfc_cat = " + ltostr(cat.lngcat);
    pg_result rs = db.exec(strsql);

    // Check # of returned rows:
    if (rs.size() != 1) my_throw("Found " + itostr(rs.size()) + " " + strdescr + " records for segment " + ltostr(lngfc_seg) + ", expected 1!");

    // Fetch the category name and directory:
    sub_cat.strname = rs.field("strname");
    sub_cat.strdir  = rs.field("strdir");
  }
  else {
    // strsub_cat lists a sub-directory or m3u file:
    if (!file_exists(strsub_cat) && !dir_exists(strsub_cat) && strsub_cat != "LineIn" && strsub_cat != "/dev/cdrom") my_throw("Could not find a file or directory called '" + strsub_cat + "'");
    sub_cat.strname = strsub_cat;
  }
}

// A recursive function used to load m3u files that contain directories, and directories which contain m3us:
// Also applies special logic to format clock sub-category directories
void segment::recursive_add_to_string_list(vector <string> & file_list, const string & strsource, const int intrecursion_level, pg_connection & db, const player_config & config) {
  // Call a recursive function to load directories, m3us, etc. Directories can contain M3U files
  // And M3U files can list directories. Go down to a maximum of 3 levels of recursion. Also, if
  // we encounter a directory, we check if it is one of the format clock-subdirectories. If it is, then
  // apply special logic to see which files to actually use (relevant from, until, etc)

  // Bomb out if our recursion level is too low (ie, this function was called incorrectly)
  if (intrecursion_level < 0) LOGIC_ERROR;

  // LineIn or CD-ROM?
  if (strsource == "LineIn") {      // LineIn?
    file_list.push_back(strsource);
  }
  else if (strsource == "/dev/cdrom") { // CD-ROM?
    // Fetch the tracks on the cdrom
    // Logic ripped & adapted from XMMSs cdaudio library
    try {
      const int CDOPENFLAGS = O_RDONLY | O_NONBLOCK;
      int fd = CHECK_LIBC(open(strsource.c_str(), CDOPENFLAGS), "open: " + strsource);
      struct cdrom_tochdr tochdr;
      CHECK_LIBC(ioctl(fd, CDROMREADTOCHDR, &tochdr), "Error querying " + strsource + " for audio tracks");

      // We have the # of the 1st and last tracks. Store playlist entries:
      for (int i = tochdr.cdth_trk0; i <= tochdr.cdth_trk1; i++) {
        //file_list.push_back(strsource + "/" + pad_left(itostr(i), '0', 2) + "-track.cdr");
        file_list.push_back("/cdrom/Track " + pad_left(itostr(i), '0', 2) + ".cda");
      }
    } catch_exceptions;
  } else if (strsource == "MusicProfile") {
    // A "MusicProfile" source means check the database for a music profile scheduled to play now.
    if (intrecursion_level > 0) {
      // Check for & load a music profile into the playlist:
      add_music_profile_to_string_list(file_list, intrecursion_level - 1, db, config);
    }
    else {
      // Can't process a Music Profile, have already reached the recurion limit!
      log_warning("Not processing Music Profile. I am already at my maxiumum search depth.");
    }
  } else if (file_is_cd_track(strsource)) { // A CD track file?
    file_list.push_back(strsource);
  } else if (dir_exists(strsource)) { // A directory?
    // One of the format clock sub-category directories?
    string strdir = ensure_last_char(strsource, '/');
    string strsql = "SELECT lngfc_sub_cat FROM tlkfc_sub_cat WHERE strdir = " + psql_str(strdir);
    pg_result rs = db.exec(strsql);
    if (rs.size() > 0) {
      long lngfc_sub_cat = strtol(rs.field("lngfc_sub_cat"));

      // A format clock sub-category directory. Fetch relevant MP3s from the database:
      string strsql_date = "'" + format_datetime(date(), "%F") + "'"; // Current date, in psql form.
      string strsql = "SELECT strfile FROM tblfc_media WHERE "
                      "lngsub_cat = " + ltostr(lngfc_sub_cat) + " AND "
                      "COALESCE(dtmrelevant_until, '9999-12-25') >= " + strsql_date;
      // Now modify the query, using the Max Age and Premature segment settings.
      if (!blnpremature) { // blnpremature means ignore relevant from
        strsql += " AND COALESCE(dtmrelevant_from, '0000-01-01') <= " + strsql_date;
      }

      if (blnmax_age) { // Max age means maximum # of days after dtmrelevant_from that the media will be played.
        strsql += " AND COALESCE(dtmrelevant_from, '9999-12-25') + " + itostr(intmax_age - 1) + " >= " + strsql_date;
      }

      // Order the media by filename:
      strsql += " ORDER BY strfile";
      pg_result rs = db.exec(strsql);

      // Did we get anything?
      if (rs.size() == 0) {
        // No:
        log_warning("Database does not list any usable format clock sub-category media under this directory: " + strdir);
        return;
      }

      // Process records:
      {
        int intadded=0; // Number if items we've added to the file list
        while (rs) {
          // Fetch the file from the database:
          string strfile = rs.field("strfile", "");
          // Exists on the harddrive?
          if (file_exists(strdir + strfile)) {
            // Yes. Add it.
            file_list.push_back(strdir + strfile);
            ++intadded;
          }
          else {
            // No. Log a warning:
            log_warning("File listed in the database, but not found on disk: " + strdir + strfile);
          }
          rs++;
        }
        // Did we add any entries?
        if (intadded <= 0) {
          // No. Log a warning.
          log_warning("Could not find any usable format clock sub-category media under this directory: " + strdir);
        }
      }
    }
    else {
      // Not a format clock sub-category directory. Process all the files.
      int intadded = 0; // Number of files we used under this directory

      // Load all MP3 files into the list
      {
        dir_list dir(strdir, ".mp3", DT_REG | DT_LNK);
        while (dir) {
          string strfile = dir;
          file_list.push_back(strdir + strfile);
          ++intadded;
        }
      }

      // Load all M3U files into the list (but only if our recursion level is high enough)
      dir_list dir(strdir, ".m3u", DT_REG | DT_LNK);
      while (dir) {
        string strfile = dir;
        if (intrecursion_level > 0) {
          // Process contents of the M3U file:
          recursive_add_to_string_list(file_list, strdir + strfile, intrecursion_level - 1, db, config);
          ++intadded;
        }
        else {
          // Can't process the M3U file, have reached recursion level.
          log_warning("Not processing M3U file " + strdir + strfile + ". I am already at my maxiumum search depth.");
        }
      }

      // Log a warning if we didn't find any files:
      if (intadded == 0) {
        log_warning("Didn't find any usable files under this directory: " + strdir);
      }
    }
  } else if (file_exists(strsource)) { // A file?
    // What is the file extension?
    string strext=lcase(right(strsource, 4));
    if (strext==".mp3") {
      // An MP3 file. Add it to the list:
      file_list.push_back(strsource);
    }
    else if (strext==".m3u") {
      // A M3U file. Are we at our recursion level?
      if (intrecursion_level <= 0) {
        // Yes. We can't process it.
        log_warning("Not processing M3U file " + strsource + ". I am already at my maxiumum search depth.");
      }
      else {
        // No. Attempt to open and read lines from the file:
        ifstream m3u_file(strsource.c_str());
        if (!m3u_file) {
          log_warning("Unable to open M3U file: " + strsource);
        }
        else {
          // Process all lines in the M3U file:
          int intadded = 0; // Lines we've used from the file:
          string strline="";
          while (getline(m3u_file, strline)) {
            // Skip empty lines and lines beginning with #:
            if (strline != "" && strline[0] != '#') {
              // Line should be fine. Process the line:
              recursive_add_to_string_list(file_list, strline, intrecursion_level - 1, db, config);
              ++intadded;
            }
          }
          // Did we get any usable lines?
          if (intadded <= 0) {
            log_warning("Didn't find any usable lines in M3U file: " + strsource);
          }
        }
      }
    }
    else {
      // Invalid file extension. Log a warning
      log_warning("I don't recognise the '" + strext + "' extension on this file: " + strsource);
    }
    return; // Done with the file source handling.
  }
  // Could not find the source:
  else log_warning("Source not found: \"" + strsource + "\"");
}

/// Utility function for segment::add_music_profile_to_string_list()
bool check_short_weekday(const string & strShortWeekDay, int & intWeekDay) {
  // Return true if strDay is mon, tue, wed, etc, and populate intWeekday with the weekday number - 1, 2, 3, etc.
  bool blnresult = false;
  intWeekDay = -1;

  // Search for the string
  const string ShortWeekDays[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  int i = 0; // Index into the ShortWeekDays array.

  for (i=0; i < 7 && ShortWeekDays[i] != strShortWeekDay; ++i);

  // Was the string found?
  if (i < 7) {
    // Yes. Return the index (+1) and a success value
    intWeekDay = i+1;
    blnresult = true;
  }
  return blnresult;
}

void segment::add_music_profile_to_string_list(vector <string> & file_list, const int intrecursion_level, pg_connection & db, const player_config & config) {
  // Check the database for a music profile that wants to play now.
  // If a music profile can't be found then load default music instead.
  // -> Logic originally from non-format clocks player, player::CheckMusicProfile()

  // Bomb out if our recursion level is too low (ie, this function was called incorrectly)
  if (intrecursion_level < 0) LOGIC_ERROR;

  long lngprofile_highest  = -1; // Database index of the highest tblmusicprofiles.lngprofile found so far.
  string strProfileName    = ""; // Name of the profile to use... (empty = Default mp3 repository)
  string strNewMusicSource = ""; // Source of music for the new profile

  // ==========================================================================================
  // Check #1 - Check tblmusicprofile fields (strstartday, dtmstarttime, strendday, dtmendtime)
  // ==========================================================================================
  {
    bool blnprofile_found = false; // Set to true when a matching music profile is found

    string strSQL = "SELECT * FROM tblMusicProfiles WHERE bitEnabled = '1' ORDER BY lngprofile DESC";
    pg_result rs = db.exec(strSQL);

    while (rs && !blnprofile_found) {
      // Set some flags for this iteration:
      bool blnskip_profile = false; // Set to true if for some reason this profile must be skipped (error, not applicable, etc)

      // Get the details
      strProfileName        = rs.field("strProfileName", "");
      string strStartDay    = rs.field("strStartDay", "");
      datetime dtmStartTime = parse_psql_time(rs.field("dtmStartTime", "0001-01-01 00:00:00"));
      string strEndDay      = rs.field("strEndDay", "");
      datetime dtmEndTime   = parse_psql_time(rs.field("dtmEndTime",   "0001-01-01 23:59:59"));
      string strMusic       = rs.field("strMusic", "");
      long lngprofile       = strtol(rs.field("lngprofile", "-1"));

      int intStartWeekDay    = -1;
      int intEndWeekDay      = -1;
      bool blnStartIsWeekDay = false;
      bool blnEndIsWeekDay   = false;
      bool blnOnStartDay     = false;
      bool blnOnEndDay       = false;

      // Attempt to interpret the details.

      // Check the start day
      // - Is it empty? (ie, probably the profile is scheduled using tblmusicprofile_date, etc records)
      if (strStartDay=="") {
        // Empty start day. Ignore the profile in this loop, it will be found by the next section if it is meant to be activated.
        blnskip_profile = true;
      }

      if (!blnskip_profile) {
        // If the start day string is not empty, then all the other fields should be valid. Check them as normal, and
        // report errors.
        if (isdate(strStartDay)) {
          blnStartIsWeekDay = false;
        } // if (isdate(strStartDay))
        else {
          // It's not a date. Is it a week-day?
          strStartDay = lcase(strStartDay);
          strStartDay = strStartDay.substr(0, 3);
          if (check_short_weekday(strStartDay, intStartWeekDay)) {
            blnStartIsWeekDay = true;
          }
          else {
            // Not a date or week-day - error! Go onto the next profile
            log_error("Error with music profile \"" + strProfileName + "\" (invalid start day)");
            blnskip_profile = true; // Don't check this particular profile any further...
          } // if (!check_short_weekday(strStartDay, intStartWeekDay))
        } // else
      }

      // Skip the next part if there was a profile error earlier...
      if (!blnskip_profile) {
        // Check the end day
        if (isdate(strEndDay)) {
          blnEndIsWeekDay = false;
        } // if (isdate(strEndDay))
        else {
          strEndDay = lcase(strEndDay);
          strEndDay = strEndDay.substr(0, 3);

          // Check if strEndDay is a valid weekday name, and find which weekday number it is...
          if (check_short_weekday(strEndDay, intEndWeekDay)) {
            blnEndIsWeekDay = true;
          }
          else {
            // Not a date or week-day - error! Go onto the next profile
            log_error("Error with profile \"" + strProfileName + "\" (invalid end day)");
            blnskip_profile = true; // Don't check the profile any further, skip and go to the next profile.
          } // if (!check_short_weekday(strEndDay, intEndWeekDay))
        } // else
      } // if (!blnskip_profile)

      bool blnDayCorrect = false; // This is set to true if the current date is within the profile's start and end day

      if (!blnskip_profile) {
        // Now that we have details of the start and end days, compare.
        blnDayCorrect = false;

        if (blnStartIsWeekDay != blnEndIsWeekDay) {
          // Must both be date or both weekday
          log_error("Error with profile \"" + strProfileName + "\" (start day type does not match end day type) ");
          blnskip_profile = true; // Skip this profile and go to the next one.
        } // if (blnStartIsWeekDay != blnEndIsWeekDay)
      } // (!blnskip_profile)

      if (!blnskip_profile) {
        if (blnStartIsWeekDay) {
          // Compare the weekdays
          int intWeekDay = weekday(date());

          if (intStartWeekDay > intEndWeekDay) {
            // Weekday must not be between the 2
            blnDayCorrect = (intWeekDay >= intStartWeekDay) || (intWeekDay <= intEndWeekDay);
          } // if (intStartWeekDay > intEndWeekDay)
          else {
            // Weekday must be between the 2
            blnDayCorrect = (intWeekDay >= intStartWeekDay) && (intWeekDay <= intEndWeekDay);
          } // end else

          blnOnStartDay = (intWeekDay == intStartWeekDay);
          blnOnEndDay = (intWeekDay == intEndWeekDay);
        } // if (blnStartIsWeekDay)
        else {
          // Compare the dates.
          // - Convert the years of the dates to this year
          datetime dtmStartDay, dtmEndDay;
          dtmStartDay = parse_date_string(strStartDay);
          dtmEndDay = parse_date_string(strEndDay);

          int intNowYear, intNowMonth, intNowDay,
              intStartYear, intStartMonth, intStartDay,
              intEndYear, intEndMonth, intEndDay;

          get_date_parts(now(), intNowYear, intNowMonth, intNowDay);
          get_date_parts(dtmStartDay, intStartYear, intStartMonth, intStartDay);
          get_date_parts(dtmEndDay, intEndYear, intEndMonth, intEndDay);

          int intYearDiff = intStartYear - intNowYear;
          intStartYear -= intYearDiff;
          intEndYear -= intYearDiff;

          dtmStartDay = make_date(intStartYear, intStartMonth, intStartDay);
          dtmEndDay  = make_date(intEndYear, intEndMonth, intEndDay);

          // A small correction: IN case this has moved the start day after today, but actually the start
          // date must be last year and this is before the end date.
          if (dtmStartDay > now()) {
            intStartYear--;
            intEndYear--;
            dtmStartDay = make_date(intStartYear, intStartMonth, intStartDay);
            dtmEndDay   = make_date(intEndYear, intEndMonth, intEndDay);
          } // if (dtmStartDay > now())

          // Now check the dates
          blnDayCorrect = ((date() >= dtmStartDay) && (date() <= dtmEndDay));

          blnOnStartDay = (date() == dtmStartDay);
          blnOnEndDay   = (date() == dtmEndDay);
        } // else

        if (blnDayCorrect) {
          // The day is correct. Check the times!
          if (blnOnStartDay) {
            if (time() < dtmStartTime) {
              // Before the start time. Try the next profile
              blnskip_profile = true;
            } // if (time() < dtmStartTime)
            else if (blnOnEndDay) {
              if (time() > dtmEndTime) {
                // After the end time. Try the next profile
                blnskip_profile = true;
              } // if (time() > dtmEndTime)
            } // else if (blnOnEndDay)
          } // if (blnOnStartDay)
          if (!blnskip_profile) {
            // === We have found a profile. Use it and quit the loop
            strNewMusicSource = strMusic;
            lngprofile_highest = lngprofile; // This is the highest "matching" lngprofile value found so far...
            blnprofile_found = true;
          } // if (!blnskip_profile)
        } // if (blnDayCorrect)
      } // if (!blnskip_profile)
      rs++;
    } // while ((!RS.eof()) && (!blnprofile_found))
  }

  // ===================================================================================
  // Check #2 - Check tblmusicprofile_date and tblmusicprofile_timezone for any profiles
  //            that have been *scheduled* to play in this hour.
  // ===================================================================================
  {
    // Fetch the related tlktimezone record index...
    long lngtimezone = -1; // The index of the timezone record...

    // tlktimezone time fields do not contain seconds. ie no matching records will be returned when
    // the current time is hh:59:ss - where ss is any second after 00.
    // - Therefore - fetch the current time, and truncate the seconds.
    datetime dtmApproxTime = (time() / 60) * 60; // datetime vars are stored as # seconds.
    string psqlApproxTime  = time_to_psql(dtmApproxTime);

    string strsql = "SELECT lngtimezone FROM tlktimezone WHERE dtmtzfrom <= " + psqlApproxTime + " AND " + psqlApproxTime + " <= dtmtzto";
    pg_result rs = db.exec(strsql);

    if (rs.size() != 1) {
      // Expected 1 record to be found!
      log_error("Error with tlktimezone table data. This query produced " +itostr(rs.size()) + " records where 1 was expected: " + strsql);
      lngtimezone = -1;
    } // if (RS.recordcount() != 1)
    else {
      // 1 record was found.
      lngtimezone = strtol(rs.field("lngtimezone"));
    } // else

    // Now search for the most recently-added profile that was scheduled to play on this date & hour(timezone):
    strsql = "SELECT tblmusicprofiles.lngprofile, tblmusicprofiles.strprofilename, tblmusicprofiles.strmusic "
             "FROM tblmusicprofiles "
             "INNER JOIN tblmusicprofile_date ON tblmusicprofiles.lngprofile = tblmusicprofile_date.lngprofile "
             "INNER JOIN tblmusicprofile_timezone ON tblmusicprofile_date.lngprofile_date = tblmusicprofile_timezone.lngprofile_date "
             "WHERE tblmusicprofiles.bitenabled='1' AND "
             "tblmusicprofile_date.dtmday = " + psql_date + " AND "
             "tblmusicprofile_timezone.lngtimezone = " + ltostr(lngtimezone) +
             " ORDER BY tblmusicprofile_timezone.lngprofile_timezone DESC "
             "LIMIT 1";
    rs = db.exec(strsql);

    // Was a tblmusicprofiles record retrieved for this date & hour(timezone)?
    if (rs) {
      // A tblmusicprofiles record was retrieved for this hour.
      long lngprofile = strtol(rs.field("lngprofile","-1"));
      strProfileName  = rs.field("strprofilename", "");
      string strMusic = rs.field("strmusic", "");

      // Compare this profile with the highest lngprofile found so far (ie, any retrieved from Check #1)
      if (lngprofile > lngprofile_highest) {
        // The lngprofile of this profile is higher than that of the profile found earlier (if one was found).
        // - So use this profile instead, because it was created more recently.
        strNewMusicSource = strMusic;
        lngprofile_highest = lngprofile; // This is the highest "matching" lngprofile value found so far...
      } // if (lngprofile > lngprofile_highest)
    } // if (!RS.eof())
  }

  if (strNewMusicSource == "") {
    // This means that no matching profile was found.
    strNewMusicSource = config.strdefault_music_source;
    strProfileName = "";
  } // if (strNewMusicSource == "")

  // Attempt to load the music profile:
  bool blnload_success = false; // Set to true if we successfully load a playlist
  log_message("Loading music profile \"" + (strProfileName=="" ? "default" : strProfileName) + "\": " + strNewMusicSource);
  {
    int intsize_before = file_list.size();
    recursive_add_to_string_list(file_list, strNewMusicSource, intrecursion_level, db, config);
    if (file_list.size() > intsize_before)
      blnload_success = true;
  }

  // If there was an error, see if we can fall back to default music...
  if (!blnload_success && (strNewMusicSource != config.strdefault_music_source)) {
    // There was an error creating the random playlist... attempt to fall back to the default music location...
    int intsize_before = file_list.size();
    log_message("Error loading playlist, attempting to use default music location: " + config.strdefault_music_source);
    recursive_add_to_string_list(file_list, config.strdefault_music_source, intrecursion_level, db, config);
    if (file_list.size() > intsize_before)
      blnload_success = true;
  }

  // If there was an error loading the default music location, see if we can fall back to the
  // default mp3 repository directory:
  if (!blnload_success && (strNewMusicSource != config.dirs.strmp3)) {
    // There was an error creating the random playlist... attempt to fall back to the default music location...
    log_message("Error creating playlist, attempting to use default mp3 repository: " + config.dirs.strmp3);
    int intsize_before = file_list.size();
    recursive_add_to_string_list(file_list, config.dirs.strmp3, intrecursion_level, db, config);
    if (file_list.size() > intsize_before)
      blnload_success = true;
  }

  // If the music playlist has still not been successfully loaded, then log this as a critical error..
  if (!blnload_success) {
    log_error("Could not create a music playlist! Music will not play correctly!");
    strProfileName = "ERROR"; // For later when we update tblliveinfo
  }

  // Do a quick update of the liveinfo table to reflect the current profile.
  write_liveinfo_setting(db, "Music profile", (strProfileName=="") ? "Default profile" : strProfileName);
}

void segment::list_music_bed_media(pg_connection & db) {
  // populate music_bed_media (lists the music media to play in this segment)
  music_bed_media.clear();
  string strsql = "SELECT strfile, strdir FROM tblfc_media INNER JOIN tlkfc_sub_cat ON tblfc_media.lngsub_cat = tlkfc_sub_cat.lngfc_sub_cat WHERE lngsub_cat = " + music_bed.strsub_cat;
  pg_result rs = db.exec(strsql);
  if (rs.size() == 0) my_throw("Could not find music bed media in the database (lngsub_cat=" + music_bed.strsub_cat + ")!");

  while(rs) {
    string strfile = ensure_last_char(rs.field("strdir"), '/') + rs.field("strfile");
    if (!file_exists(strfile)) {
      log_warning("Music bed media listed in database but not found on disk: " + strfile);
    }
    else {
      music_bed_media.push_back(strfile);
    }
    rs++;
  }

  // Check if we have any music bed files:
  if (music_bed_media.size() == 0) my_throw("Could not find any Music Bed media!");

  // Shuffle the list:
  srand(now());
  random_shuffle(music_bed_media.begin(), music_bed_media.end(), myrand);

  // Now setup the iterator:
  music_bed_media_it = music_bed_media.begin();
}

string segment::get_music_bed_media() {
  // Fetch the next listed music bed item:
  if (music_bed_media.size() == 0) LOGIC_ERROR;

  string strret = *music_bed_media_it;
  ++music_bed_media_it;

  if (music_bed_media_it == music_bed_media.end()) {
    music_bed_media_it=music_bed_media.begin();
  }

  return strret;
}

