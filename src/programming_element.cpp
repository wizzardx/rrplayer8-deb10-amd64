
#include "programming_element.h"
#include "common/file.h"
#include "common/logging.h"
#include "common/my_string.h"

// Constructor
programming_element::programming_element() {
  reset(); // Reset data in this object
}

// Destructor
programming_element::~programming_element() {
}

// Reset data in this object
void programming_element::reset() {
  blnloaded = false; // Data has not been loaded into this object.
  cat      = SCAT_SILENCE;
  strmedia = "";
  strvol   = "";
  blnmusic_bed = false;
  music_bed.strmedia    = "";
  music_bed.strvol      = "";
  dtmstarted = datetime_error;
  music_bed.intstart_ms = -1;
  music_bed.intlength_ms = -1;
  music_bed.already_handled.blnstart = false;
  music_bed.already_handled.blnstop = false;
  promo.lngtz_slot = -1;
  promo.blnforced_time = false;
  end.blnloaded = false;
  end.intlength_ms = -1;
  end.intend_silence_start_ms = -1;
  end.blndynamically_compressed = false;
  end.intend_quiet_start_ms = -1;
  end.blnends_with_fade = false;
}

// Load information about the mp3 end from the database (tblinstore_media)
void programming_element::load_end(pg_connection & db) {
  // Some basic checks:
  if (end.blnloaded) {
    log_warning("Ending info for " + strmedia + " already loaded");
    return;
  }
  if (strmedia == "LineIn") {
    log_warning("Not loading ending details for " + strmedia);
    return;
  }
  if (lcase(right(strmedia, 4)) != ".mp3") {
    log_warning("Not loading ending details for non-mp3 " + strmedia);
    return;
  }
  // Look for ending info in tblinstore_media:
  // - Split the file into dirname and basename:
  string strdirname, strbasename;
  break_down_file_path(strmedia, strdirname, strbasename);
  // - Query:
  pg_result rs = db.exec("SELECT intlength_ms, intend_silence_start_ms, "
    "blndynamically_compressed, intend_quiet_start_ms, blnends_with_fade "
    "FROM tblinstore_media JOIN tblinstore_media_dir USING "
    "(lnginstore_media_dir) WHERE strdir = " + psql_str(strdirname) + " AND "
    "strfile = " + psql_str(strbasename) + " AND intlength_ms IS NOT NULL");
  if (!rs) {
    log_warning("No information for end of " + strmedia + " in database!");
    return;
  }
  // We found information, so load it:
  end.intlength_ms = strtoi(rs.field("intlength_ms", "-1"));
  end.intend_silence_start_ms = strtoi(rs.field("intend_silence_start_ms", "-1"));
  end.intlength_ms = strtoi(rs.field("intlength_ms", "-1"));
  end.intend_silence_start_ms = strtoi(rs.field("intend_silence_start_ms", "-1"));
  end.blndynamically_compressed = strtobool(rs.field("blndynamically_compressed", "f"));
  end.intend_quiet_start_ms = strtoi(rs.field("intend_quiet_start_ms", "-1"));
  end.blnends_with_fade = strtobool(rs.field("blnends_with_fade", "f"));
  end.blnloaded = true;

  // Log a warning at this point if the mp3 is a song and not dynamically compressed;
  if (cat == SCAT_MUSIC && !end.blndynamically_compressed) {
    log_warning("Song " + strmedia + " is not dynamically range compressed! I can't tell if it gradually fades out.");
  }
}

// A global variable containing the previous music segment's programming element list
programming_element_list prev_music_seg_pel;

// A cache of programming element lists, used when we are in a hurry to get a playlist:
// A class used by generate_playlist to remember recent programming element lists.
cpel_cache pel_cache; // Global variable for player storage of recent programming element lists

void cpel_cache::clear() {
  cache.clear();
}

bool cpel_cache::has(const string & id) {
  tidy();
  cache_type::iterator i = cache.begin();
  return i != cache.end();
}

bool cpel_cache::get(const string & id, programming_element_list & pel) {
  tidy();
  // Check if there is a cached record for the source:
  {
    cache_type::const_iterator i = cache.find(id);
    if (i == cache.end()) {
      // Could not find it.
      return false;
    }
    else {
      // Found it:
      pel = i->second.pel;
      return true;
    }
  }
}

void cpel_cache::set(const string & id, const programming_element_list & pel) {
  pel_info pi;
  pi.pel = pel;
  pi.cached_time = now();
  cache[id] = pi;
}

void cpel_cache::tidy() {
  // Tidy up old records (older than 6 hours):
  cache_type::iterator i = cache.begin();
  while (i != cache.end()) {
    // Store the current iterator value and go to the next
    // (iterators are invalidated when the current record is deleted)
    cache_type::iterator i_old = i++;
    // Old record?
    if (i_old->second.cached_time < now() - (6*60*60)) {
      // Yes. Delete it:
      cache.erase(i_old);
    }
  }
}

