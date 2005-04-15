#include "segment.h"
#include "common/testing.h"
#include "common/file.h"
#include "common/dir_list.h"
#include "common/my_string.h"

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
    setup_as_music_profile(strdefault_music_source, "<Default Music Profile>");
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

  // Alternative category
  alt_cat.lngcat  = strtol(rs.field("lngalt_cat", "-1"));
  alt_cat.strname = rs.field("stralt_cat_name", "");
  alt_cat.cat     = parse_category_string(alt_cat.strname);

  // Sub-category
  load_sub_cat_struct(sub_cat, rs.field("strsub_cat"), db, cat, lngfc_seg, "Sub-category", "strsub_cat");

  // Alternative sub-category:
  load_sub_cat_struct(alt_sub_cat, rs.field("stralt_sub_cat"), db, alt_cat, lngfc_seg, "Alternative sub-category", "stralt_sub_cat");

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

void segment::setup_as_music_profile(const string & strmusic_source, const string & strdesc) {
  // Segment-specific info
  cat.cat     = SCAT_MUSIC;
  cat.strname = "Music";
  
  sequence       = SSEQ_RANDOM; // Play music in random order
  blnpromos      = true; // Allow announcement playback during this segment.
  blnrepeat      = true; // Music
  blncrossfading = true; // Music items crossfade into each other.

  // Now generate a music playlist from the specified location:
  generate_music_playlist(programming_elements, strmusic_source); // Also shuffles the list

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
        log_message("Ran out of media, going back to the beginning of the playlist.");
        next_item = programming_elements.begin();
      }
      else {
        // Repeating not allowed. Depending on the time left at this point, we either fill it with a
        // "imaging filler", or we revert to the alternate category.

        // How much time remains until the end of the segment?
        int inttime_before_seg_end = intlength - (now() - dtmstart) - intstarts_ms/1000;
        
        if (inttime_before_seg_end <= intuse_imaging_filler_limit) {
          // Look for imaging filler.
          log_line("Ran out of media for this segment (repeat=false). Fetching an imaging filler for the remaning " + itostr(inttime_before_seg_end) + "s");
          get_imaging_filler(pe, inttime_before_seg_end, db);
        }
        else {
          // Revert to the alternate category (or to the default music profile if we're already on the alternate category).
          log_line("Ran out of media for this segment (repeat=false). " + itostr(inttime_before_seg_end) + "s remain so reverting...");
          revert_down(db, strdefault_music_source); // This will also setup "next_item"
        }
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

// Fetch an imaging filler (used when 1) Repeat=false, 2) We've just run out of items to play, and 3) There is not much time left in the segment.
void segment::get_imaging_filler(programming_element & pe, const int inttime_before_seg_end, pg_connection & db) {
  undefined_throw;
}

void segment::generate_music_playlist(programming_element_list & pel, const string & strsource) {
  // Process a directory or M3U file and generate a list of music to play during this segment.
  pel.clear(); // Clear anything already in the program element list.

  // Is the source a file ?
  if (file_exists(strsource)) {
    undefined_throw;
  }
  // Is it a directory?
  else if (dir_exists(strsource)) {
    // Load all MP3 files into the PEL
    {
      dir_list dir(strsource, ".mp3", DT_REG | DT_LNK);
      string strfile = dir;
      while (strfile != "") {
        programming_element pe;
        pe.cat = SCAT_MUSIC;
        pe.strmedia = strsource + strfile;
        pe.strvol   = "MUSIC";
        pe.blnloaded = true;
        pel.push_back(pe);
        strfile = dir;
      }
    }
    // Load all M3U files into the PEL
    {
      dir_list dir(strsource, ".m3u", DT_REG | DT_LNK);
      string strfile = dir;
      while (strfile != "") {
        undefined_throw;
        strfile = dir;
      }
    }
  }
  // Is it "LineIn"?
  else if (strsource == "LineIn") {
    undefined_throw;
  }
  else my_throw("Unknown music source: \"" + strsource + "\"");

  // Check how many items there are in the playlist:
  if (pel.size() <= 0) {
    // Throw an exception if there are no entries:
    my_throw("Could not find any media for the music playlist: \"" + strsource + "\"");
  }
  else if (pel.size() < 10) {
    // Log a warning if there are very few entries:
    log_warning("The new music playlist only has " + itostr(pel.size()) + " song(s)!");
  }

  // And at the end shuffle the music playlist:
  shuffle_pel(pel);
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

  // The Music category gets special handling:
  if (cat.cat == SCAT_MUSIC) {
    // Music

    // List media to play for this segment.
    generate_music_playlist(pel, sub_cat.strsub_cat);

    // And shuffle the PEL at the end of the function:
    blnshuffle_pel = true;
  }
  else {
    // Not music. Everything besides music has uniform handling.
    // Is the sequence "Specific"? (ie, use only 1 media item)
    if (sequence == SSEQ_SPECIFIC) {
      // Prepare a single programming element and add it to the list.
      programming_element pe;
      pe.cat = cat.cat;
      if (!file_exists(strspecific_media)) my_throw("\"Specific\" media not found: " + strspecific_media);
      pe.strmedia=strspecific_media;
      pe.strvol="PROMO"; // Code for use "PROMO" (announcement) volume.
      if (blnmusic_bed) {
        pe.blnmusic_bed    = true;
        pe.music_bed.strmedia = get_music_bed_media(strtol(music_bed.strsub_cat), db);
        pe.music_bed.strvol   = "MUSIC";
        pe.music_bed.intstart_ms = 0; // Not yet using this functionalit.
        pe.music_bed.intlength_ms = INT_MAX; // Not yet using this functionality.
      }
      pe.blnloaded = true;
      pel.push_back(pe);
    }
    else { // Random or Sequence
      // Prepare a list of all the (valid) media from under the sub-category.
      // The basic query:
      string strsql_date = "'" + format_datetime(date(), "%F") + "'"; // Current date, in psql form.
      string strsql = "SELECT strfile FROM tblfc_media WHERE "
                      "lngcat = " + ltostr(cat.lngcat) + " AND "
                      "lngsub_cat = " + sub_cat.strsub_cat + " AND "
                      "dtmrelevant_until >= " + strsql_date;
      // Now modify the query, using the Max Age and Premature segment settings.
      if (!blnpremature) { // blnpremature means ignore relevant from
        strsql += " AND dtmrelevant_from <= " + strsql_date;
      }

      if (blnmax_age) { // Max age means maximum # of days after dtmrelevant_from that the media will be played.
        strsql += " AND dtmrelevant_from + " + itostr(intmax_age - 1) + " >= " + strsql_date;
      }

      // Order the media by filename:
      strsql += " ORDER BY strfile";
      pg_result rs = db.exec(strsql);

      // Check the number of records returned:
      if (rs.recordcount() == 0) my_throw("There are no usable media for this sub-category!");

      // Build the Programming Element list:
      while (!rs.eof()) {
        // Check if the file exists before adding it to our PEL:
        string strfile = sub_cat.strdir + rs.field("strfile", "");
        if (!file_exists(strfile)) {
testing;
          // File not found!
          log_warning("Media not found: " + strfile);
testing;
        }
        else {
          // File found, add it:
          programming_element pe;
          pe.cat = cat.cat;
          pe.strmedia=strfile;
          pe.strvol="PROMO"; // Code for use "PROMO" (announcement) volume.
          if (blnmusic_bed) {
            pe.blnmusic_bed    = true;
            pe.music_bed.strmedia = get_music_bed_media(strtol(music_bed.strsub_cat), db);
            pe.music_bed.strvol   = "MUSIC";
            pe.music_bed.intstart_ms = 0; // Not yet using this functionalit.
            pe.music_bed.intlength_ms = INT_MAX; // Not yet using this functionality.
          }
          pe.blnloaded = true;
          pel.push_back(pe);
        }
        rs.movenext();
      }

      // Did we get any files? (maybe they're all missing):
      if (pel.size() == 0) my_throw("All files in this sub-category are missing!");

      // Do we shuffle the media?
      blnshuffle_pel = (sequence == SSEQ_RANDOM);
    }
  }

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
    switch (playback_state) {
      case PBS_CATEGORY: {
        log_message("Reverting to Alternative Category & Sub-Category");
        playback_state = PBS_ALTERNATE;
        try {
          load_pe_list(programming_elements, cat, sub_cat, db);
          next_item = programming_elements.begin();
        } catch_exceptions;
      } break;
      case PBS_ALTERNATE: {
        log_message("Reverting to Default Music Profile");
        playback_state = PBS_DEFAULT_MUSIC;
        try {
          setup_as_music_profile(strdefault_music_source, "<Default Music Profile>");
          // Also allow promos to play now:
          blnpromos = true;
        } catch_exceptions;
      } break;
      case PBS_DEFAULT_MUSIC: {
        testing;
        my_throw("There was a problem with the Default music profile, but there is nothing else to play!");
      } break;
      default: my_throw("Logic Error!");
    }
    blndone = next_item != programming_elements.end();
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

  if (cat.cat == SCAT_MUSIC) {
    // Special handling if the segment category is Music
    // Need to check if strsub_cat points to a tblmusicprofiles record, a directory, or a playlist file:
    if (isint(strsub_cat)) {
      // A music profiles record
      undefined_throw;
    }
    else if (strsub_cat == "") { // Empty!
testing;      
      log_error("A Music category segment (" + ltostr(lngfc_seg) + ") has an emtpy sub-category!");
    }
    else if (!file_exists(strsub_cat) && !dir_exists(strsub_cat)) {
testing;
      log_error("A Music category segment (" + ltostr(lngfc_seg) + ") has a non-existant source: \"" + strsub_cat + "\"");
    }
  }
  else {
    // Sub-category *must* represent a valid integer:
    if (!isint(strsub_cat)) my_throw("Format Clock Segment " + ltostr(lngfc_seg) + " has an invalid " + strfield + " value \"" + strsub_cat + "\"");

    // Fetch category details:
    string strsql = "SELECT strname, strdir FROM tlkfc_sub_cat WHERE lngfc_sub_cat = " + strsub_cat + " AND lngfc_cat = " + ltostr(cat.lngcat);
    pg_result rs = db.exec(strsql);

    // Check # of returned rows:
    if (rs.recordcount() != 1) my_throw("Found " + itostr(rs.recordcount()) + " " + strdescr + " records for segment " + ltostr(lngfc_seg) + ", expected 1!");

    // Fetch the category name and directory:
    sub_cat.strname = rs.field("strname");
    sub_cat.strdir  = rs.field("strdir");
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
