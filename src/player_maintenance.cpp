// Timed player maintenance events. Run when there is spare time during playback.

#include "player.h"
#include "common/exception.h"
#include "common/file.h"
#include "common/string_hash_set.h"
#include "common/string_splitter.h"
#include "common/my_string.h"
#include "common/temp_dir.h"
#include <fstream>

void player::player_maintenance(const int intmax_time_ms) {
  // Do background maintenance (separate function). Events have frequencies, (sometimes desired "second" to take place at), and are prioritiesed.
  //  - Also includes resetting info about the next playback item (highest priority, every 30 seconds..., seconds: 00, 30)
//  log_line("I have up to " + itostr(intmax_time_ms/1000) + "s to do maintenance in...");
  datetime dtmcutoff = now() + intmax_time_ms / 1000; // Logic not allowed to run past this length.

  #define RUN_TIMED(FUNC, FREQ) { \
    static datetime dtmlast = datetime_error; \
    if (now() < dtmcutoff) { \
      datetime dtmnow = now(); \
      if (dtmlast/(FREQ) != dtmnow/(FREQ)) { \
        FUNC(dtmcutoff); \
        dtmlast = now(); \
      } \
    } \
  }

  // These events run immediately, and then only after their
  // frequency (in seconds) has elapsed:
  RUN_TIMED(maintenance_check_received,     10);
  RUN_TIMED(maintenance_check_waiting_cmds, 10);
  RUN_TIMED(maintenance_operational_check,  30);
  RUN_TIMED(maintenance_player_running,     60);
  RUN_TIMED(maintenance_hide_xmms_windows,  5*60); // Hide all XMMS windows
}

void player::maintenance_check_received(const datetime dtmcutoff) {
  // Run this logic only if we have 60s or more remaining:
  if (dtmcutoff >= now() + 60) {
    check_received();
  }
}

void player::maintenance_check_waiting_cmds(const datetime dtmcutoff) {
  // Run this logic only if we have 60s or more remaining:
  if (dtmcutoff >= now() + 60) {
    process_waiting_cmds();
  }
}

void player::maintenance_operational_check(const datetime dtmcutoff) {
  // Update the live-info file every 10 minutes;
  // - Only if more than 20 seconds remain:
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
  if (run_data.blnlog_all_music_to_db && run_data.current_segment.cat.cat == SCAT_MUSIC && dtmcutoff >= now() + 120) {
    // Log an informative message.
    log_message("Music playlist was updated, writing to database...");
    // Log the playlist to the DB
    log_music_playlist_to_db();
    
    // Also do a quick scan of all the music on the machine, and log this to the database
    log_message("Scanning machine for music...");
    log_machine_avail_music_to_db();
    
    // Done now:
    run_data.blnlog_all_music_to_db = false;
  }
  
  // If the cached mp3 tags have changed, write them to disk now:
  mp3tags.save_changes();
}

void player::maintenance_player_running(const datetime dtmcutoff) {
  // Display a line (once every minute) showing that the player is running...
  log_line("Running...");
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
    string strtitle = pe->strmedia;
    
    if (strtitle != "LineIn") {
      // An MP3 then.
      strtitle = mp3tags.get_mp3_description(strtitle);
    }
      
    string strmessage = ""; ///< Goes into tblplayeroutput.strmessage
    if (strtitle == "") {
      strmessage = pe->strmedia + "||" + strtitle;
    }
    else {
      strmessage = pe->strmedia + "||" + get_short_filename(pe->strmedia);
    }
      
    string strsql = "INSERT INTO tblplayeroutput (strmessage, strmsgdesc, dtmtime) VALUES (" + psql_str(strmessage) + ", " + psql_str(strPlaylistDescr) + ", " + psql_time + ")";
    transaction.exec(strsql);
    
    pe++;
  }
  
  // No problems, so commit the database transaction:
  transaction.commit();
}

void player::log_machine_avail_music_to_db() {
  // Scan the harddrive for available music, and log to the database.
  
  // Create a postgresql transaction. We're going to be doing a lot of updates:
  pg_transaction transaction(db);

  const string strAvailMP3sDescr = "avail_mus";
  const string strDisabledMP3sDescr = "disabled";

  // Remove all avail_mus records
  string strSQL = "DELETE FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strAvailMP3sDescr);
  transaction.exec(strSQL);
  
  // Make an in-memory list of all the disabled MP3s
  strSQL = "SELECT strmessage FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strDisabledMP3sDescr);
  pg_result RS = db.exec(strSQL);

  // This is where we store the loaded and decoded details:
  string_hash_set DisabledMP3s;

  while (!RS.eof()) {
    string_splitter split(RS.field("strmessage", ""), "||");
    string strDisabledMP3 = trim(split);

    if (strDisabledMP3 != "") {
      DisabledMP3s.insert(strDisabledMP3);
    }
    RS.movenext();
  }
  
  // A temporary directory to list our available mp3s into:
  temp_dir avail_music_dir("avail_music");
  string stravail_list_file = (string) avail_music_dir + "avail_music.txt";
  
  // Build up a linux command to list all of the machine's music MP3s into a text-file
  // - The textfule is "avail_music.txt"
  string strCommand = "ls " + config.dirs.strmp3 +  "*.[Mm][Pp]3 > " + stravail_list_file + 
                     "; find " + config.dirs.strprofiles + " | grep \"\\.[Mm][Pp]3\" >> " + stravail_list_file;
  system(strCommand.c_str());

  // Open playlist.m3u and read all the lines. Extract the mp3 filename out of the paths
  ifstream AvailMusicFile(stravail_list_file.c_str());
  if (AvailMusicFile) {
    char ch_FileLine[2048] = "";

    while (AvailMusicFile.getline(ch_FileLine, 2047)) {
      string strLine = trim(ch_FileLine);

      // Now skip listing this file under "available music" if it is listed under "disabled music"
      if (!key_in_string_hash_set(DisabledMP3s, strLine)) {
        // Now that we have the line, attempt to get the MP3 title
        string strTitle = mp3tags.get_mp3_description(strLine);

        // Decide on the final line, and also prepend the path and ||
        if (strTitle != "")
          strLine = strLine + "||" + strTitle;
        else
          strLine = strLine + "||" + get_short_filename(strLine);

        if (strLine != "") {
          strSQL = "INSERT INTO tblplayeroutput (strmessage, strmsgdesc, dtmtime) VALUES (" + psql_str(strLine) + ", " + psql_str(strAvailMP3sDescr) + ", " + psql_time + ")";
          transaction.exec(strSQL);
        }
      }
    }
    AvailMusicFile.close();
  } else log_error("Could not open: " + stravail_list_file);

  // No problems, so commit the database transaction:
  transaction.commit();
}
