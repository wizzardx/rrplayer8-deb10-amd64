// Timed player maintenance events. Run when there is spare time during playback.

#include "player.h"
#include "common/exception.h"
#include "common/file.h"
#include "common/string_hash_set.h"
#include "common/string_splitter.h"
#include "common/my_string.h"
#include "common/temp_dir.h"
#include "common/linein.h"
#include <fstream>
#include "common/rr_misc.h"

void player::player_maintenance(const int intmax_time_ms) {
  // Do background maintenance (separate function). Events have frequencies, (sometimes desired "second" to take place at), and are prioritiesed.
  //  - Also includes resetting info about the next playback item (highest priority, every 30 seconds..., seconds: 00, 30)
//  log_line("I have up to " + itostr(intmax_time_ms/1000) + "s to do maintenance in...");
  datetime dtmcutoff = now() + intmax_time_ms / 1000; // Logic not allowed to run past this length.

  // These events run immediately, and then only after their
  // frequency (in seconds) has elapsed:
  RUN_TIMED_CUTOFF(maintenance_check_received(dtmcutoff),     10,   dtmcutoff);
  RUN_TIMED_CUTOFF(maintenance_check_waiting_cmds(dtmcutoff), 10,   dtmcutoff);
  RUN_TIMED_CUTOFF(maintenance_operational_check(dtmcutoff),  30,   dtmcutoff);
  RUN_TIMED_CUTOFF(maintenance_player_running(dtmcutoff),     60,   dtmcutoff);
  RUN_TIMED_CUTOFF(maintenance_hide_xmms_windows(dtmcutoff),  5*60, dtmcutoff); // Hide all XMMS windows
}

void player::maintenance_check_received(const datetime dtmcutoff) {
  // Run this logic only if we have 30s or more remaining:
  if (dtmcutoff >= now() + 30) {
    check_received();
  }
}

void player::maintenance_check_waiting_cmds(const datetime dtmcutoff) {
  // Run this logic only if we have 10s or more remaining:
  if (dtmcutoff >= now() + 10) {
    process_waiting_cmds();
  }
}

void player::maintenance_operational_check(const datetime dtmcutoff) {
  // Update the live-info table every 10 minutes;
  if (dtmcutoff >= now() + 60) {
    static datetime dtmlast_run = datetime_error;
    if (now()/(10*60) != dtmlast_run/(10*60)) {
      try {
        write_liveinfo();
      } catch_exceptions;
      dtmlast_run = now();
    }
  }

  // If the music playlist changed, then the global variable [run_data.blnlog_all_music_to_db] is set. Here is where
  // we actually log all the available music on the machine.
  if (run_data.blnlog_all_music_to_db && run_data.current_segment.cat.cat == SCAT_MUSIC && dtmcutoff >= now() + 60) {
    // Log an informative message.
    log_message("Music playlist was updated, writing to database...");
    // Log the playlist to the DB
    try {
      log_music_playlist_to_db();
    } catch_exceptions;

    // Done now:
    run_data.blnlog_all_music_to_db = false;
  }

  // Check for ads that have been missed (they were meant to play but it's been too long since the correct time.
  write_errors_for_missed_promos();

  // If the cached mp3 tags have changed, write them to disk now:
  mp3tags.save_changes();

  // Approximately every 30 seconds we update tblplayeroutput & tblliveinfo with the current
  // playback status:
  log_mp_status_to_db();
}

void player::maintenance_player_running(const datetime dtmcutoff) {
  // Display a line (once every minute) showing that the player is running...
  string strline = "Running... (";
  if (run_data.current_item.cat == SCAT_SILENCE) {
    strline += "Silence";
  }
  else if (run_data.current_item.strmedia == "LineIn") {
    strline += "LineIn: " + itostr(linein_getvol()) + "%";
  }
  else {
    int intsession = run_data.get_xmms_used(SU_CURRENT_FG);
    strline += "xmms " + itostr(intsession) + ": " + itostr(run_data.xmms[intsession].getvol()) + "%";

    // Fetch the volume of the music bed, if one is being used:
    try {
      intsession = run_data.get_xmms_used(SU_CURRENT_BG); // Try to fetch the session
      // No exception thrown, so we have an xmms session for the music bed.
      strline += ". Music bed: xmms " + itostr(intsession) +": " + itostr(run_data.xmms[intsession].getvol()) + "%";
    } catch(...) {}
  }

  // If the next playback event is more than an 24 hours in the future then don't
  // print it. Probably the end time is undefined (eg: silence, linein).
  {
    long lngseconds_remaining = dtmcutoff - now();
    if (lngseconds_remaining < 24*60*60) {
      // Time of the next playback event is approximate. It sometimes falls short by 1 second.
      strline += ". Next playback event: ~" + format_datetime(now() + lngseconds_remaining + 1, "%T") + " (" + itostr(lngseconds_remaining + 1) +"s)";
    }
  }
  strline += ")";
  log_line(strline);
}

void player::maintenance_hide_xmms_windows(const datetime dtmcutoff) {
  // Hide all visible XMMS windows.
  for (int intsession=0; intsession < intmax_xmms; intsession++) {
    run_data.xmms[intsession].hide_windows();
  }
}

// Functions called by maintenance_operational_check:
void player::log_music_playlist_to_db() {
  // Log the contents of the current music playlist to the database
  // - Check that the current segment is loaded and has a music category:
  if (!run_data.current_segment.blnloaded) my_throw("Current Format Clock segment is not loaded!");
  if (run_data.current_segment.cat.cat != SCAT_MUSIC) my_throw("Current Segment does not have a MUSIC category");

  // Create a postgresql transaction. We're going to be doing a lot of updates:
  pg_transaction transaction(db);

  const string strPlaylistDescr = "playlist";

  // Remove all existing playlist records:
  string strsql = "DELETE FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strPlaylistDescr);
  transaction.exec(strsql);

  // Now proceed through the playlist:
  programming_element_list::const_iterator pe = run_data.current_segment.programming_elements.begin();

  while (pe != run_data.current_segment.programming_elements.end()) {
    try {
      string strfile = pe->strmedia;
      string strtitle = mp3tags.get_mp3_description(strfile);

      string strlength = "N/A";
      try {
        strlength = itostr(mp3tags.get_mp3_length(strfile));
      } catch(...) {}

      string strmessage = strfile + "||" + strtitle + "||" + strlength; ///< Goes into tblplayeroutput.strmessage
      string strsql = "INSERT INTO tblplayeroutput (strmessage, strmsgdesc, dtmtime) VALUES (" + psql_str(strmessage) + ", " + psql_str(strPlaylistDescr) + ", now())";
      transaction.exec(strsql);
    } catch_exceptions;
    pe++;
  }

  // No problems, so commit the database transaction:
  transaction.commit();
}
