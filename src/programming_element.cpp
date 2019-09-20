
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
  // Additional info, provided by media maintenance:
  // - General info
  media_info.blnloaded = false;
  media_info.intlength_ms = -1;
  media_info.blndynamically_compressed = false;
  // - MP3 beginning info
  media_info.intbegin_silence_stop_ms = -1;
  media_info.intbegin_quiet_stop_ms = -1;
  media_info.blnbegins_with_fade = false;
  // - Ending info
  media_info.intend_silence_start_ms = -1;
  media_info.intend_quiet_start_ms = -1;
  media_info.blnends_with_fade = false;
}

// Load information about the mp3 end from the database (tblinstore_media)
void programming_element::load_media_info(pg_conn_exec & db) {
  // Some basic checks:
  if (media_info.blnloaded) {
    log_warning("Media information for " + strmedia + " already loaded");
    return;
  }
  if (strmedia == "LineIn") {
    log_debug("Not loading media information for " + strmedia);
    return;
  }
  if (lcase(right(strmedia, 4)) != ".mp3") {
    log_debug("Not loading media information for non-mp3 " + strmedia);
    return;
  }
  // Look for media info in tblinstore_media:
  // - Split the file into dirname and basename:
  string strdirname, strbasename;
  break_down_file_path(strmedia, strdirname, strbasename);
  // - Query:
  string sql =
    "SELECT intlength_ms, intend_silence_start_ms, "
    "blndynamically_compressed, intend_quiet_start_ms, blnends_with_fade, "
    "intbegin_silence_stop_ms, intbegin_quiet_stop_ms, blnbegins_with_fade "
    "FROM tblinstore_media JOIN tblinstore_media_dir USING "
    "(lnginstore_media_dir) WHERE strdir = ? AND "
    "strfile = ? AND intlength_ms IS NOT NULL";
  pg_params params = ARGS_TO_PG_PARAMS(psql_str(strdirname),
                                       psql_str(strbasename));
  ap_pg_result rs = db.exec(sql, params);
  if (!*rs) {
    log_warning("No information for end of " + strmedia + " in database!");
    return;
  }
  // We found information, so load it:
  // - General info
  media_info.intlength_ms = strtoi(rs->field("intlength_ms", "-1"));
  media_info.blndynamically_compressed = strtobool(rs->field("blndynamically_compressed", "f"));
  // - MP3 beginning info
  media_info.intbegin_silence_stop_ms = strtoi(rs->field("intbegin_silence_stop_ms", "-1"));
  media_info.intbegin_quiet_stop_ms = strtoi(rs->field("intbegin_quiet_stop_ms", "-1"));
  media_info.blnbegins_with_fade = strtobool(rs->field("blnbegins_with_fade", "f"));
  // - MP3 ending info
  media_info.intend_silence_start_ms = strtoi(rs->field("intend_silence_start_ms", "-1"));
  media_info.intend_quiet_start_ms = strtoi(rs->field("intend_quiet_start_ms", "-1"));
  media_info.blnends_with_fade = strtobool(rs->field("blnends_with_fade", "f"));
  // All fields loaded successfully, so record as loaded:
  media_info.blnloaded = true;

  // Log a warning at this point if the mp3 is a song and not dynamically compressed;
  if (cat == SCAT_MUSIC && !media_info.blndynamically_compressed) {
    log_warning("Song " + strmedia + " is not dynamically range compressed! I can't tell if it gradually fades out.");
  }
}

// A global variable containing the previous music segment's programming element list
programming_element_list prev_music_seg_pel;
