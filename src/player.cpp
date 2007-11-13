/***************************************************************************
                          player.cpp  -  The main control object
                             -------------------
    begin                : Wed Mar 16 2005
    copyright            : (C) 2005 by David Purdy
    email                : david@radioretail.co.za
 *******************************************f********************************/

#include "player.h"
#include "config.h"
#include "player_util.h"
#include "common/config_file.h"
#include "common/dir_list.h"
#include "common/exception.h"
#include "common/file.h"
#include "common/linein.h"
#include "common/rr_date.h"
#include "common/rr_misc.h"
#include "common/rr_misc_db.h"
#include "common/rr_security.h"
#include "common/system.h"

#include "common/testing.h"

namespace xmmsc = xmms_controller;

player * pplayer = NULL; // A pointer to the player object, used by callback functions.

// Information about "events" that take place during playback of the current item.
// (current item ends, music bed starts, music bed starts

// Constructor
playback_events_info::playback_events_info() {
  reset();
}

void playback_events_info::reset() {
  // Set the attributes to "safe" values, ie no events are going to happen in the near future.
  intnext_ms             = INT_MAX;
  intitem_ends_ms        = INT_MAX;
  intmusic_bed_starts_ms = INT_MAX;
  intmusic_bed_ends_ms   = INT_MAX;
  intpromo_interrupt_ms  = INT_MAX;
  intrpls_interrupt_ms   = INT_MAX;
  inthour_change_interrupt_ms = INT_MAX;
}

// A function we use with the sort() algorithm:
bool transition_event_less_than(const transition_event & e1, const transition_event & e2) {
  return e1.intrun_ms < e2.intrun_ms;
}

// Constructor:
player::player() {
  // Throw an exception if there is already a player object instantiated:
  if (pplayer != NULL) my_throw("Only one player object is allowed!");

  pplayer = this;

  // Call a separate function to do the init, because kdevelop breakpoints set in the constructor don't work.
  init();
}

// Destructor:
player::~player(){
  pplayer = NULL; // If the player is destroyed, then this pointer becomes invalid...
}

// Main logic:
void player::run() {
  // Temp code for generating test data.
/*
  format_clock_test_data FCTD(db);
  FCTD.clear_tables();
  FCTD.generate_test_data();
*/

  // Wait for any items currently playing in XMMS to finish:
  try {
    log_message("Waiting for any currently-playing XMMS sessions to finish...");
    datetime dtmlast_logged=datetime_error; // We check every second, but only long once a minute.
    bool done = false; // Set to false while we're still waiting...
    while (!done) {
      done = true; // Set to false if we detect an XMMS session still running
      for (int i=0; i < intmax_xmms; i++) {
        if (xmmsc::xmms[i].playing()) {
          string strfile = xmmsc::xmms[i].get_song_file_path();
          if (dtmlast_logged/60 != now()/60)
            log_message(" - XMMS session " + itostr(i) + " is busy playing \"" + strfile + "\" - \"" + mp3tags.get_mp3_description(strfile) + "\" (" + itostr(xmmsc::xmms[i].get_song_pos()) + "/" + itostr(xmmsc::xmms[i].get_song_length()) + "s)...");
          done = false;
        }
      }
      if (!done) {
        dtmlast_logged = now(); // Put here so we will log info for all XMMS sessions still playing, not just the first one
        sleep(1);
      }
    } while (!done);
  } catch_exceptions;

  // Reset data used during this loop:
  run_data.init();

  while (true) {
    bool blnsuccess = false; // Set to true at the end of each iteration where no exceptions are trapped
    try {
      // Sleep 1 second
      sleep (1);

      // Check playback status of XMMS, LineIn, etc. Throw errors here if there is something wrong.
      check_playback_status();

      // Fetch info about how long it is to go until: (item ends, music bed ends, music bed starts, next event)
      playback_events_info playback_events;
      // If a promo is going to interrupt the music, we will first fade out the music, ie it will only start after
      // [intcrossfade_length_ms] milliseconds have elapsed.
      get_playback_events_info(playback_events, config.intcrossfade_length_ms);

      int intnext_playback_safety_margin_ms = get_next_playback_safety_margin_ms();
      if (playback_events.intnext_ms > intnext_playback_safety_margin_ms) {
        // If we have enough time left (> Safety margin, or unknown), then:
        // - Do background maintenance (separate function). Has built-in timing.
        player_maintenance(playback_events.intnext_ms - intnext_playback_safety_margin_ms);
      }
      else {
        // We're close to one or more a playback events. Handle them in an intensive timing section.
        playback_transition(playback_events);
      }
      blnsuccess=true; // No exception took place this iteration
    } catch_exceptions;

    // Did we catch an exception this iteration?
    if (!blnsuccess) {
      // Reset playback:
      log_error("Playback reset is now required.");
      run_data.init();
    }
  }
}

void player::log(const log_info & LI) {
  // Log to the logfile & to database:
  rr_log_file(LI, PLAYER_LOG_FILE);
}

// Enable or disable extra messages sent to cout...
void player::debug(const bool _blndebug) {
  blndebug = _blndebug;
}

void player::init() {
  // Log the player version:
  rr_log_prog_starting();

  // Reset the object attributes
  reset();

  // Change to the correct folder right away (use a hard-coded value because the DB is not open yet)
  chdir(PLAYER_DIR);

  if (process_instances("player") > 1) {
    my_throw("Player already loaded, terminating...");
  }

  // Log the today's RR date.
  log_line("Today's RR date: " + datetime_to_rrdate(date()));

  // Read database connection settings from the Players config file
  log_line("Reading config file");

  // Read DB connection settings from a config file
  read_config_file();

  // Log that we're about to connect to the instore database.
  log_line("Connecting to instore database (" + config.db.strdb + " on " + config.db.strserver + ")...");

  // Get schedule database connection string.
  string strconn = pg_create_conn_str(
    config.db.strserver,
    config.db.strport,
    config.db.strdb,
    config.db.struser,
    config.db.strpassword);

  // Setup a function to be called by the db object when the connection to the database fails (ie, keep music going):
  db.call_on_connect_error(callback_check_db_error);

  // Now attempt to connect to the database, and retry until successful
  db.open(strconn);

  // If the player is in debugging mode then say so:
  if (blndebug) log_warning("Running in debug mode");

  // Reload all config settings from the database:
  load_db_config();

  // Now generate some test data...
/*  format_clock_test_data FCTD(db);
  FCTD.clear_tables();
  FCTD.generate_test_data();*/

  // Clear any media player commands (pause, stop, resume) that are
  // in the database and still waiting to execute...
  log_message("Removing waiting commands: mppa, mpst, mpre...");
  remove_waiting_mediaplayer_cmds();

  // Fetch the current store status:
  log_message("Checking store status...");
  load_store_status(true);

  // Write LiveInfo data:
  log_message("Writing LiveInfo data...");
  write_liveinfo();

  // Check the received folder
  log_message("Checking the received folder...");
  check_received();              // Check the received folder, scan for CMD files

  // Added in version 6.06 - if the player is not busy playing ads, then check if there are
  // any ads that are marked as about to play. It can happen that ads are queued to play,
  // but the player application is stopped. The database will continue to show these ads as
  // 'waiting' but they will never play. Every time this proc runs, check for these
  // announcements and correct them
  log_message("Checking for invalid 'waiting' promos...");
  correct_waiting_promos();

  // Write errors for missed announcements...
  log_message("Checking for missed promos...");
  write_errors_for_missed_promos();

  // Load music history:
  log_message("Loading music history...");
  m_music_history.load(db);

  // Also init the mp3 tags:
  mp3tags.init(PLAYER_DIR + "mp3_tags.txt");

  // Setup the XMMS module:
  xmmsc::set_num_xmms_sessions(intmax_xmms); // 2 XMMS sessions

  // Show that the init succeeded.
  log_message("Player startup complete.");
  log_message("Player main event loop starting...");
  log_line("");
}

void player::reset() {
  // Reset ALL object attributes to default, uninitialized values.

  // Database connection:
  config.db.strserver   = "";
  config.db.strdb       = "";
  config.db.struser     = "";
  config.db.strpassword = "";
  config.db.strport     = "";

  // Promo frequency-capping:
  config.intmins_to_miss_promos_after = -1;
  config.intmax_promos_per_batch      = -1;
  config.intmin_mins_between_batches  = -1;

  // Directories:
  config.dirs.strmp3           = "";
  config.dirs.stradverts       = "";
  config.dirs.strannouncements = "";
  config.dirs.strspecials      = "";
  config.dirs.strreceived      = "";
  config.dirs.strtoday         = "";
  config.dirs.strprofiles      = "";

  // Default music profiles source:
  config.strdefault_music_source = "";

  // Wait for the current song to end before starting promo playback? Exception: linein music & "force to play now" promos.
  config.blnpromos_wait_for_song_end = false;

  // Format clock settings:
  config.blnformat_clocks_enabled = false;
  config.lngdefault_format_clock  = false;

  // Crossfade settings:
  config.intcrossfade_length_ms = 8000; // Defaults to 8 seconds.

  // Store's current status:
  store_status.blnopen = false;
  store_status.volumes.intmusic    = -1;
  store_status.volumes.intannounce = -1;
  store_status.volumes.intlinein   = -1;
  store_status.volumes.dblxmmseqpreamp = -1;

  // Debugging output:
  blndebug = false;
}


void player::remove_waiting_mediaplayer_cmds() {
  // Remove waiting MediaPlayer commands (pause, stop, resume, etc)
  db.exec("UPDATE tblWaitingCMD SET bitComplete = '1', bitError = '1', dtmProcessed = " + psql_now + " WHERE ((lower(strcommand)='mppa') OR (lower(strcommand)='mpst') OR (lower(strcommand)='mpre')) AND ((bitComplete = '0') OR (bitComplete IS NULL))");
}

void player::callback_check_db_error() {
  // A callback function called by the db (database connection) object when there is a database
  // connection problem. It keeps music going, etc.
  undefined_throw;
}

void player::read_config_file() {
  // Read settings from the player config file, into the config.db structure
  config_settings cfg; // Details read from the file.
  load_config_file_section(PLAYER_DIR + PACKAGE + ".conf", "database", cfg);

  // Check that all settings were found:
  if (cfg["server"]   == "") my_throw("\"server\" setting not found");
  if (cfg["db"]       == "") my_throw("\"db\" setting not found");
  if (cfg["user"]     == "") my_throw("\"user\" setting not found");
  if (cfg["password"] == "") my_throw("\"password\" setting not found");
  if (cfg["port"]     == "") my_throw("\"server\" setting not found");

  // Now copy the settings into the player's config.db structure:
  config.db.strserver   = cfg["server"];
  config.db.strdb       = cfg["db"];
  config.db.struser     = cfg["user"];
  config.db.strpassword = decrypt_string(cfg["password"], get_rr_encrypt_key(), 2);
  config.db.strport     = cfg["port"];
}

void player::load_db_config() {
  // Load all the other settings (besides config.db) into the config structure.

  // Promo frequency capping options
  config.intmins_to_miss_promos_after = strtoi(load_tbldefs(db, "intMissUnplayedAdsAfter",  "15", "int"));
  config.intmax_promos_per_batch      = strtoi(load_tbldefs(db, "intMaxAdsPerBatch",        "3", "int"));
  config.intmin_mins_between_batches  = strtoi(load_tbldefs(db, "intMinTimeBetweenAdBatch", "4", "int"));

  // CHECK:
  if (config.intmins_to_miss_promos_after <= 0 || config.intmins_to_miss_promos_after >= 10000) {
    const int intdefault = 15;
    config.intmins_to_miss_promos_after = intdefault;
    log_error("tbldefs:intMissUnplayedAdsAfter has an invalid value. Defaulting to " + itostr(intdefault));
  }

  if (config.intmax_promos_per_batch <= 0 || config.intmax_promos_per_batch > 10) {
    const int intdefault = 3;
    config.intmax_promos_per_batch = intdefault;
    log_error("tbldefs:intMaxAdsPerBatch has an invalid value. Defaulting to " + itostr(intdefault));
  }

  if (config.intmin_mins_between_batches < 0 || config.intmin_mins_between_batches > 60) {
    const int intdefault = 4;
    config.intmin_mins_between_batches = intdefault;
    log_error("tbldefs:intMinTimeBetweenAdBatch has an invalid value. Defaulting to " + itostr(intdefault));
  }

  // Directories
  {
    pg_result rs = db.exec("SELECT strmp3, stradverts, strannouncements, strspecials, strreceived, strtoday, strprofiles FROM tblapppaths");
    if (rs.size() != 1) log_error("Invalid number of records (" + itostr(rs.size()) + ") found in tblapppaths!");
    config.dirs.strmp3           = ensure_last_char(rs.field("strmp3"), '/');
    config.dirs.stradverts       = ensure_last_char(rs.field("stradverts"), '/');
    config.dirs.strannouncements = ensure_last_char(rs.field("strannouncements"), '/');
    config.dirs.strspecials      = ensure_last_char(rs.field("strspecials"), '/');
    config.dirs.strreceived      = ensure_last_char(rs.field("strreceived"), '/');
    config.dirs.strtoday         = ensure_last_char(rs.field("strtoday"), '/');
    config.dirs.strprofiles      = ensure_last_char(rs.field("strprofiles"), '/');
  }

  // CHECK:
  if (!dir_exists(config.dirs.strmp3))           log_error("MU directory not found: " + config.dirs.strmp3);
  if (!dir_exists(config.dirs.stradverts))       log_error("AD directory not found: " + config.dirs.stradverts);
  if (!dir_exists(config.dirs.strannouncements)) log_error("CA directory not found: " + config.dirs.strannouncements);
  if (!dir_exists(config.dirs.strspecials))      log_error("SP directory not found: " + config.dirs.strspecials);
  if (!dir_exists(config.dirs.strreceived))      log_error("Received directory not found: " + config.dirs.strreceived);
  if (!dir_exists(config.dirs.strtoday))         log_error("Today directory not found: "    + config.dirs.strtoday);
  if (!dir_exists(config.dirs.strprofiles))      log_error("Profiles directory not found: "    + config.dirs.strprofiles);

  // Default music source
  config.strdefault_music_source = load_tbldefs(db, "strDefaultMusicSource", config.dirs.strmp3, "str");

  // CHECK:

  // Check tblapppaths.strmp3 - it should not be the same as tblapppaths.strprofiles
  //  - if it is then this means that the Wizard has probably set a music source
  //    (linein/cd music, etc) in a deprecated way. Correct strmp3 and update the default
  //    music source setting.

  const string strcorrect_mp3_path = "/data/radio_retail/stores_software/data/";

  if (config.dirs.strmp3 != strcorrect_mp3_path) {
    log_warning("Detected: The Wizard used a deprecated method for setting music type! Correcting...");

    // Remember the incorrect strmp3 setting as the default music source setting...
    config.strdefault_music_source = config.dirs.strmp3;
    save_tbldefs(db, "strDefaultMusicSource", "str", config.strdefault_music_source);

    // Now correct tblapppaths.strmp3
    config.dirs.strmp3 = strcorrect_mp3_path;
    db.exec("UPDATE tblapppaths SET strmp3 = " + psql_str(config.dirs.strmp3));
  }

  // Do promos that want to play, wait for the current song to end?
  config.blnpromos_wait_for_song_end = strtobool(load_tbldefs(db, "blnAdvertsWaitForSongEnd", "false", "bln"));

  // Format clock settings
  config.blnformat_clocks_enabled = strtobool(load_tbldefs(db, "blnFormatClocksEnabled", "false", "bln"));

  // Only load the "default" format clock setting if Format Clocks are enabled:
  if (config.blnformat_clocks_enabled) {
    config.lngdefault_format_clock = strtoi(load_tbldefs(db, "lngDefaultFormatClock", "-1", "lng"));
    // CHECK:
    pg_result rs = db.exec("SELECT lngfc FROM tblfc WHERE lngfc = " + itostr(config.lngdefault_format_clock));
    if (rs.size() != 1) log_error("Invalid tbldefs:lngDefaultFormatClock value! Found " + itostr(rs.size()) + " matching Format Clock records!");
  }

  // Read the crossfade length:
  config.intcrossfade_length_ms = strtoi(load_tbldefs(db, "intCrossfadeLength", "8000", "int"));

  // Check the setting:
  if (config.intcrossfade_length_ms < 500) {
    log_warning("Config setting for Crossfade length (" + itostr(config.intcrossfade_length_ms) + "ms) is too short! (defaulting to 500ms)");
    config.intcrossfade_length_ms = 500;
  }
  else if (config.intcrossfade_length_ms > 30000) {
    log_warning("Config setting for Crossfade length ( " + itostr(config.intcrossfade_length_ms) + "ms) is too long! (defaulting to 30,000ms)");
    config.intcrossfade_length_ms = 30000;
  }
}

void player::load_store_status(const bool blnverbose, const bool blnforceload) {
  // Make sure this function doesn't run too regularly...
  {
    static datetime dtmlast_run = datetime_error;
    datetime dtmnow = now();

    // Check for problems caused by clock changes:
    if (dtmlast_run > dtmnow) {
      log_message("System clock was set backwards, adjusting logic");
      dtmlast_run = datetime_error;
    }

    // Now check if it is too soon to run the logic again:
    // (once every 30s) (run anyway if blnforceload is set)
    if (!blnforceload && (dtmlast_run/30 == dtmnow/30)) return;

    dtmlast_run = dtmnow;
  }

  // Update the current store status

  // Is the store open now?
  {
    string strsql = "SELECT dtmOpeningTime, dtmClosingTime FROM tblStoreHours WHERE intDayNumber = " + itostr(weekday(now()));
    pg_result rs = db.exec(strsql);
    if (rs.size() != 1) my_throw("An error with table tblstorehours. Query returned " + itostr(rs.size()) + " rows! (expected 1)");
    datetime dtmopen  = parse_psql_time(rs.field("dtmopeningtime"));
    datetime dtmclose = parse_psql_time(rs.field("dtmclosingtime"));

    datetime dtmtime = time();

    // Check if the store's open/closed state changes:
    bool blnprev_store_open = store_status.blnopen;

    if (dtmopen <= dtmclose) {
      store_status.blnopen = (dtmopen <= dtmtime) && (dtmtime <= dtmclose);
    }
    else { // Weird cases, eg: Store opens at 11 PM and closes at 2 AM.
      store_status.blnopen = !((dtmtime >= dtmclose) && (dtmtime <= dtmopen));
    }

    // Did the store status change?
    if (blnverbose || blnprev_store_open != store_status.blnopen) {
      string strmessage = (string) "Store is now " + (store_status.blnopen ? "Open" : "Closed");
      strmessage += (string) " (store hours: " + format_datetime(dtmopen, "%T") + " - " + format_datetime(dtmclose, "%T") + ")";
      log_message(strmessage);
    }
  }

  // If the store is closed, then reset all the volumes to 0 %:
  if (!store_status.blnopen) {
    store_status.volumes.intmusic    = 0;
    store_status.volumes.intannounce = 0;
    store_status.volumes.intlinein   = 0;
    store_status.volumes.dblxmmseqpreamp = 0;
  }
  else {
    // Store is open now. Fetch the current store volumes.
    // * Fetch the current adjustment volume
    int intadjust_vol = 0;
    {
      string strsql = "SELECT * FROM tblVolumeZones WHERE intDayNumber = " +
                      itostr(weekday(now())) + " AND lngTimeZone = " +
                      itostr(hour(now()) + 1);
      pg_result rs = db.exec(strsql);
      intadjust_vol = strtoi(rs.field("intvoladj", "0"));
    }
    // * Fetch current music & announce volumes from the database, and calculate live settings.
    {
      pg_result rs=db.exec("SELECT intmusicvolume, intannvolume from tblstore");
      if (rs.size() != 1) log_error("Invalid number of records (" + itostr(rs.size()) + ") found in tblstore!");
      // Fetch values:
      store_status.volumes.intmusic    = strtoi(rs.field("intmusicvolume", "45"));
      store_status.volumes.intannounce = strtoi(rs.field("intannvolume", "90"));

      // Adjust values:
      store_status.volumes.intmusic    += intadjust_vol;
      store_status.volumes.intannounce += store_status.volumes.intmusic;
    }

    // Fetch the linein volume:
    store_status.volumes.intlinein   = strtoi(load_tbldefs(db, "intLineInVol", "255", "int"));

    // Fetch the XMMS equalizer pre-amp (some stores need a lot of signal amp)
    store_status.volumes.dblxmmseqpreamp = strtod(load_tbldefs(db, "fltXMMSEqPreAmp", "0.0", "flt", "XMMS Equalizer Pre-amp (db)"));

    // Convert all volumes to a %
    #define CONVERT_255_100(X) X=((X*100)/255)
    CONVERT_255_100(store_status.volumes.intmusic);
    CONVERT_255_100(store_status.volumes.intannounce);
    CONVERT_255_100(store_status.volumes.intlinein);
    #undef CONVERT_255_100

    // Convert all volumes to 80% of their original volumes (prevent the highest volume settings being
    // used, they can create distortion.

    #define CONVERT_100_80(X) X=((X*80)/100)
    CONVERT_100_80(store_status.volumes.intmusic);
    CONVERT_100_80(store_status.volumes.intannounce);
    CONVERT_100_80(store_status.volumes.intlinein);
    #undef COVERT_100_80

    // Check the range of all volumes (must be from 0 to 80%)
    #define LIMIT_RANGE_0_80(X,S) { if(X<0) {log_error((string)"Invalid " + S + " level: " + itostr(X) + "%. Using 0%"); X=0;} else if (X>80) {log_error((string)"Invalid " + S + " level: " + itostr(X) + "%. Using 80%"); X=80;} }
    LIMIT_RANGE_0_80(store_status.volumes.intmusic,    "Music");
    LIMIT_RANGE_0_80(store_status.volumes.intannounce, "Announcement");
    LIMIT_RANGE_0_80(store_status.volumes.intlinein,   "LineIn");
    #undef LIMIT_RANGE_0_80
  }
}

void player::update_output_volumes() {
  // Immediately update XMMS sessions & LineIn levels according to the current store status
  // (don't wait for the next programming element to start playing)

  // Abort if the current item is not loaded:
  if (!run_data.current_item.blnloaded) LOGIC_ERROR;

  // If a "silence" item is playing, reset all volumes to 0:
  if (run_data.current_item.cat == SCAT_SILENCE) {
    // XMMS levels:
    for (int intsession=0; intsession < intmax_xmms; intsession++)
      xmmsc::xmms[intsession].setvol(0);
    // Linein level:
    linein_setvol(0);
  }
  // Otherwise, if LineIn is playing, then set it's volume:
  else if (run_data.uses_linein(SU_CURRENT_FG)) {
    linein_setvol(store_status.volumes.intlinein);
  }
  // Otherwise, set the XMMS volume:
  else {
    // An exception will be thrown if the current item does not use XMMS:
    int intsession = run_data.get_xmms_used(SU_CURRENT_FG);
    xmmsc::xmms[intsession].setvol(get_pe_vol(run_data.current_item.strvol));

    // Also set the XMMS pre-gain appropriately:
    xmmsc::xmms[intsession].set_eq_preamp(store_status.volumes.dblxmmseqpreamp);

    // Set volume of music bed also if it is active now:
    {
      int intsession = -1;
      try {
        intsession = run_data.get_xmms_used(SU_CURRENT_BG);
      } catch(...) {}
      if (intsession != -1) {
        // Still need to add logic for Music Beds
        undefined_throw;
      }
    }
  }
}

void player::write_liveinfo() {
  // Player version 6.15 - removing use of loaderinfo.tmp and liveinfo.chk.
  // liveinfo is a collection of information about the store computer's running software

  // Kill liveinfo.chk if it exists
  string LiveInfoChk = config.dirs.strtoday + "liveinfo.chk";
  if (file_exists(LiveInfoChk)) rm(LiveInfoChk.c_str());

  // Get the store IP address, store code, store name, music volume, announcement volume
  string strSQL = "SELECT * FROM tblStore";
  pg_result RS = db.exec(strSQL);

  string strMusicVol, strAnnouncementVol;
  strMusicVol = strAnnouncementVol = "";

  if (RS) {
    strMusicVol = RS.field("intmusicvolume", "DB error");
    strAnnouncementVol = RS.field("intannvolume", "DB error");
  }

  // Count the number of announcements to play today
  strSQL = "SELECT "
               "SUM(tlkDay.lngDay) AS Counter FROM tblSchedule_TZ_Slot, tblSchedule_TimeZone, "
               "tblSchedule_Date, tlkDay "
           "WHERE "
               "((tblSchedule_TZ_Slot.lngSchedTimeZone = tblSchedule_TimeZone.lngSchedTimeZone) AND "
                   "(tblSchedule_TimeZone.lngSchedDate = tblSchedule_Date.lngSchedDate)) AND "
               "(tblSchedule_Date.dtmDay = " + psql_date + ") AND "
               "(tlkDay.lngDay=1) "
           "GROUP BY tlkDay.lngDay";
  RS = db.exec(strSQL);

  string strNumAdsToday;
  if (RS) {
    strNumAdsToday = RS.field("Counter");
  }
  else {
    strNumAdsToday = "0";
  }

  // Get the description for the currently-playing profile
/*
  strSQL = "SELECT strprofilename FROM tblmusicprofiles WHERE strmusic = " + psql_str(strMusicSource);
  RS =  db.exec(strSQL);

  string strProfileName = "";

  if (!RS.eof())
    strProfileName = RS.field("strprofilename", "<Profile name error>");
  else
    strProfileName = "Default profile";
*/

  // Get string forms of the date and time
  string strDate = format_datetime(date(), "%d/%m/%Y");
  string strTime = format_datetime(time(), "%I:%M:%S %p");

  // Also update the tblLiveInfo table
  write_liveinfo_setting(db, "Date", strDate);
  write_liveinfo_setting(db, "Time", strTime);
  write_liveinfo_setting(db, "Music vol", strMusicVol);
  write_liveinfo_setting(db, "Announce vol", strAnnouncementVol);

/*
  write_liveinfo_setting("Adjustment vol", itostr(lrint(CurrentStatus.curAdjVol)));
*/

  write_liveinfo_setting(db, "Ads today", strNumAdsToday);
  write_liveinfo_setting(db, "Player version", VERSION);
/*
  write_liveinfo_setting("Music profile", strProfileName);
*/
}

void player::check_received() {
  // this procedure checks the apppaths.recieved directory for the following files to process
  // .cmd

  // Go through all files in the received folder and where necessary reset their
  // Read-Only attribute
  clear_readonly_in_dir(config.dirs.strreceived);

  // Process command files
  dir_list Dir_CMD(config.dirs.strreceived, ".cmd");
  while (Dir_CMD) {
    string FileName = Dir_CMD; // used to read file names from a folder.
    // Create the full path
    string Full_Path = config.dirs.strreceived + FileName;
    log_message("Processing CMD file: " + FileName);
    // Load the command file into the database
    try {
      load_cmd_into_db(Full_Path);
      process_waiting_cmds();
    } catch_exceptions;
    rm(Full_Path);
  }
}

void player::load_cmd_into_db(const string strfull_path) {
  undefined_throw;
}

void player::process_waiting_cmds() {
  // Process CMD command files that are sent to the store and stored in the database
  long lngWaitingCMD = -1; // Used for error logging
  string strSQL;

  try {
    bool blnVolZones = false;
    double volChange;
    string chDay, chZone, chTime;

    string psql_Time;
    string psql_Now;

    // Run through the waiting player commands
    string strSQL = "SELECT lngWaitingCMD, strCommand, strParams, dtmProcessed, bitComplete, bitError FROM tblwaitingcmd LEFT OUTER JOIN tblcmdtype USING (lngcmdtype) LEFT OUTER JOIN tblapp USING (lngapp) WHERE COALESCE(bitComplete, '0') = '0' AND lower(COALESCE(tblapp.strdescr, 'player')) = 'player'";
    pg_result rsCMD = db.exec(strSQL);

    while(rsCMD) {
      string strCommand = ucase(rsCMD.field("strCommand", ""));
      string strParams = rsCMD.field("strParams", "");
      lngWaitingCMD = strtoi(rsCMD.field("lngWaitingCMD"));

      log_message("Processing this command: \"" + strCommand + " " + strParams + "\"");

      try {
        if (strCommand=="RPLS") {
          // Added by David - 12 November 2002
          // This is a new command in player version 6.02 when this command is found, the player will instantly stop
          // playing, reload the strmp3 path (where music is expected to be found), check the current music profile,
          // rebuild the playlist from scratch, and then resume playback. This command was added so that when the Wizard
          // wants to change the current music selection, the player will instantly respond and start playing this music.
          log_message("Processing RPLS (Reload Playlist) command...");

          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");

          // 1) Reload various database & store details from the database:
          load_db_config();
          load_store_status();

          // 2) Tell the player to re-load the current segment
          //    - This also reloads the current Music Profile (if music profiles are playing)
          run_data.blnforce_segment_reload = true;

          // 3) Clear any cached playlists:
          pel_cache.clear();
        }
        // Some commands added in version 6.11 - allow the user to pause, stop and resume the media playback.
        //
        else if (strCommand=="MPPA") {
          undefined_throw;
        /*
          log_message("Processing MPPA (Media Player Pause) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          Media_Pause();
*/
        }
        else if (strCommand=="MPST") {
          undefined_throw;
          /*
          log_message("Processing MPST (Media Player Stop) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          Media_Stop();
          */
        }
        else if (strCommand=="MPRE") {
        undefined_throw;
        /*
          log_message("Processing MPRE (Media Player Resume) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          Media_Resume();
         */
        }
        else if (strCommand=="RCFG") {
          // Added in version 6.14 on 05/08/2003 - the player now loads some of it's config options from the
          // database at startup. When the player reads an "RCFG" command it will reload these settings
          log_message("Processing RCFG (Reload Config) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          load_db_config();
        }
        else if (strCommand=="RVOL") {
          // Reload volumes
          log_message("Processing RVOL (Reload Volumes) command...");

          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");

          // Reload volumes from the database:
          load_store_status(false, true); // Not verbose, force a load now (even if a load took place in the last 30 seconds).

          // Update volumes of linein & xmms sessions as appropriately:
          update_output_volumes();
        }
        else {
          // The command is unknown, report an error
          my_throw("Unknown command " + strCommand);
        }

        strSQL = "UPDATE tblWaitingCMD SET bitComplete = '1',bitError = '0',dtmProcessed = " +
                 psql_now + " WHERE lngWaitingCMD = " + rsCMD.field("lngWaitingCMD", "-1");
        db.exec(strSQL);
      }
      catch(const my_exception & E) {
        log_error("Error with this command: " + strCommand + (strParams != "" ? (string(" ") + strParams + string(" ")) : "") + " - " + E.get_error());
        strSQL = "UPDATE tblWaitingCMD SET bitComplete = '1', bitError = '1', dtmProcessed = " + psql_now + " WHERE lngWaitingCMD = " + rsCMD.field("lngWaitingCMD", "-1");
        db.exec(strSQL);
      }
      rsCMD++;
    }
  }
  catch(...) {
    strSQL = "UPDATE tblWaitingCMD SET dtmProcessed=" + psql_now + ", bitComplete='1', bitError='1' WHERE lngWaitingCMD = " + ltostr(lngWaitingCMD);
    db.exec(strSQL);
    throw;
  }
}

void player::correct_waiting_promos() {
  // Check the DB for announcements that are marked as 'waiting to play' ie, enqueued. If the player
  // Is not currently playing back announcements then there should not be any announcements
  // like this. Correct any, and log that there was a correction
  string strsql = "SELECT lngtz_slot FROM tblschedule_tz_slot WHERE bitscheduled = '" + itostr(ADVERT_LISTED_TO_PLAY) + "'";
  pg_result RS = db.exec(strsql);
  long lngcorrected = RS.size();

  // Now run a query to fix all these hanging 'waiting' announcements.
  strsql = "UPDATE tblschedule_tz_slot SET bitscheduled = '" + itostr(ADVERT_SNS_LOADED) + "' WHERE bitscheduled = '" + itostr(ADVERT_LISTED_TO_PLAY) + "'";
  db.exec(strsql);

  // Log how many were corrected
  if (lngcorrected > 0) {
    log_message(itostr(lngcorrected) + " announcement(s) corrected. (Changed status from 'about to be played' to 'loaded from SNS').");
  }
}

void player::write_errors_for_missed_promos() {
  // Look for ads from today that are older than [Config.intMinsToMissAdsAfter] minutes
  // have not yet been played (or scheduled to play)

  // Work out the earliest time today (adverts older than this have been missed)
  string psql_EarliestTime = "";
  {
    // Make sure that our earliest time does not wrap around to late in the evening!
    // - Not comparing datetime values in case after converting the earliest time to a
    // timetamp, it ends up as a later time of day anyway.
    datetime dtmnow = now();
    datetime dtmearliest = dtmnow - (60 * config.intmins_to_miss_promos_after);
    string strnow      = format_datetime(dtmnow,      "%T");
    string strearliest = format_datetime(dtmearliest, "%T");
    if (strearliest > strnow) dtmearliest = dtmnow;
    psql_EarliestTime = time_to_psql(dtmearliest);
  }

  string strSQL = "SELECT tblSchedule_TZ_Slot.lngTZ_Slot, tblSched.strFilename, tblSched.strProductCat, "
     "tblSchedule_TZ_Slot.dtmDay, tblSlot_Assign.dtmStart, tblSlot_Assign.dtmEnd, "
     "tblSched.strPriorityOriginal, tblSched.strPriorityConverted, "
     "tblSchedule_TZ_Slot.bitScheduled, tblSchedule_TZ_Slot.bitPlayed, tblSchedule_TZ_Slot.dtmForcePlayAt "
     "FROM tblSchedule_TZ_Slot "
     "INNER JOIN tblsched ON tblSchedule_TZ_Slot.lngsched=tblsched.lngschedule "
     "INNER JOIN tblSlot_Assign USING (lngassign) "
     "WHERE "

     // Time is ok?
     "("
         // Yesterday or earlier? If so then the ad is missed
         "(tblSchedule_TZ_Slot.dtmDay < " + psql_date + ") OR "

         "(" // Same day, bad time ?
             "(tblSchedule_TZ_Slot.dtmDay = " + psql_date + ") AND "
             "(tblSlot_Assign.dtmStart < " + psql_EarliestTime + ")"
         ")"
     ")"

     // Check the various bits
     " AND (tblSchedule_TZ_Slot.bitScheduled = " + itostr(ADVERT_SNS_LOADED) + ") "
     " AND (COALESCE (tblschedule_tz_slot.bitMissErrorWritten, '0') = '0')"

     // Ordering
     " ORDER BY tblSched.strFileName, tblSchedule_TZ_Slot.lngTZ_Slot";

  pg_result RS = db.exec(strSQL);

  // We need to gather statistics for each missed announcement (first time missed, last time, number)
  // and display summaries of each. The query is sorted by filename to facilitate this also.
  string strmissed_file = "";
  string strmissed_prev_file="";
  long lngmissed_count=0;
  datetime dtmmissed_first=datetime_error;
  datetime dtmmissed_last=datetime_error;

  while (RS) {
    string strTZ_Slot = RS.field("lngTZ_Slot");
    datetime dtmDay  = parse_psql_date(RS.field("dtmDay"));
    datetime dtmTime = parse_psql_time(RS.field("dtmForcePlayAt", RS.field("dtmStart", " ").c_str()));
    datetime dtmPlayAt = dtmDay + dtmTime - timezone;

    // We don't want to generate errors for each individual missed announcement. We
    // are ordering the returned results by filename, so we can gather statistics for each
    // announcement and display a  summary "missed" error.

    // - Is this the same file as the last returned, or is this the first file?
    strmissed_file = RS.field("strFileName", "ERROR");

    // Does the line list a new mp3?
    if (strmissed_file != strmissed_prev_file) {
      // This recordset lists details for a new mp3 that was missed.
      // - Are there previously gathered stats?
      if (strmissed_prev_file != "") {
        // Yes, so print out the stats gathered so far
        write_errors_for_missed_promos_log_missed(strmissed_prev_file, lngmissed_count, dtmmissed_first, dtmmissed_last);
      }
      // Now initialize the stats.
      lngmissed_count=1;
      dtmmissed_first=dtmPlayAt;
      dtmmissed_last=dtmPlayAt;
    }
    else {
      // This recordset lists details for the same missed mp3 as the last recordset that was processed.
      // - Update the stats.
      ++lngmissed_count;
      if (dtmPlayAt < dtmmissed_first) {
        dtmmissed_first = dtmPlayAt;
      }
      if (dtmPlayAt > dtmmissed_last) {
        dtmmissed_last = dtmPlayAt;
      }
    }

    // Now update the previous missed file, we need to use this value outside
    // the loop...
    strmissed_prev_file = strmissed_file;

    // We have gathered the stat details for this record. Mark the "miss error written" field true
    strSQL = "UPDATE tblSchedule_TZ_Slot SET bitMissErrorWritten = '1' WHERE lngTZ_Slot = " + strTZ_Slot;
    db.exec(strSQL);

    // Now move to the next record.
    RS++;
  }

  // Now, at the end of the loop, write the details for the last missed announcement that we
  // were processing above.
  if (strmissed_prev_file != "") {
    // Yes, so print out the stats gathered for the last processed filename.
    write_errors_for_missed_promos_log_missed(strmissed_prev_file, lngmissed_count, dtmmissed_first, dtmmissed_last);
  }
}

void player::write_errors_for_missed_promos_log_missed(const string strmissed_file, const long lngmissed_count, const datetime dtmmissed_first, const datetime dtmmissed_last) {
  string strmissed_first = format_datetime(dtmmissed_first, "%F %T");
  string strmissed_last = format_datetime(dtmmissed_last, "%F %T");

  // v6.15 - Missed adverts are logged as messages instead of errors, ie, logged but not e-mailed.
  if (lngmissed_count == 1) {
    log_warning("Announcement " + strmissed_file + " (at " + strmissed_first + ") was missed.");
  }
  else {
    log_warning("Announcement " + strmissed_file + " was missed " + itostr(lngmissed_count) + " times between " + strmissed_first + " and " + strmissed_last);
  }
}

// FUNCTIONS AND ATTRIBUTES USED DURING RUN():

void player::check_playback_status() {
  // Check playback status of XMMS, LineIn, etc. Throw errors here if there is something wrong.

  // Starup/stop XMMS sessions:
  xmmsc::ensure_correct_num_xmms_sessions_running();
  
  for (int intsession=0; intsession < intmax_xmms; intsession++) {
    // Find out what the session should be playing now, if anything:
    string strplaying = ""; // Stays "" if nothing should be playing, but gets set if something should be.
    int intvol = -1; // Set to the correct volume of the item
    switch(run_data.xmms_usage[intsession]) {
      case SU_UNUSED: break; // Nothing should be playing
      case SU_CURRENT_FG: { // Current item
        strplaying = run_data.current_item.strmedia;
        intvol = get_pe_vol(run_data.current_item.strvol);
       } break;
      case SU_CURRENT_BG: { // Current item music bed.
        strplaying = run_data.current_item.music_bed.strmedia;
        intvol = get_pe_vol(run_data.current_item.music_bed.strvol);
      } break;
      case SU_NEXT_FG: { // Next item
        strplaying = run_data.next_item.strmedia;
        intvol = get_pe_vol(run_data.next_item.strvol);
      } break;
      case SU_NEXT_BG: {  // Next item music bed.
        strplaying = run_data.next_item.music_bed.strmedia;
        intvol = get_pe_vol(run_data.current_item.music_bed.strvol);
      } break;
      default: LOGIC_ERROR; // This should never run.
    }

    // Is the XMMS session in use?
    if (run_data.xmms_usage[intsession] == SU_UNUSED) {
      // XMMS session is not used
      // XMMS must be stopped
      if (!xmmsc::xmms[intsession].stopped()) my_throw("XMMS session " + itostr(intsession) + " is meant to be in a 'stopped' state!");
    }
    else {
      // XMMS session is used. Check that it's still playing
      if (!xmmsc::xmms[intsession].playing()) my_throw("XMMS session " + itostr(intsession) + " is meant to be playing!");
      // Check that it's playing the correct media
      if (xmmsc::xmms[intsession].get_song_file_path() != strplaying) my_throw("XMMS session " + itostr(intsession) + " is playing incorrect media!");
      // Check that the volume is correct
      {
        int intxmms_vol = xmmsc::xmms[intsession].getvol();
        if (intxmms_vol != intvol) my_throw("XMMS session " + itostr(intsession) + " has an incorrect volume! (" + itostr(intxmms_vol) + "% instead of " + itostr(intvol) + "%)");
      }
      // Check that repeat is off.
      if (xmmsc::xmms[intsession].getrepeat()) my_throw("XMMS session " + itostr(intsession) + " repeat is turned on!");
    }
  }

  // Check LineIn:
  {
    bool blnplaying = false; // Set to true if linein should be playing, false if not.
    switch (run_data.linein_usage) {
      case SU_UNUSED:     blnplaying = false; break;
      case SU_CURRENT_FG: blnplaying = true;  break;
      case SU_CURRENT_BG: blnplaying = true;  break;
      case SU_NEXT_FG:    blnplaying = true;  break;
      case SU_NEXT_BG:    blnplaying = true;  break;
      default: LOGIC_ERROR; // This should never run
    }

    // Now calculate the volume LineIn should be at:
    int intcorrect_linein_vol = (blnplaying ? store_status.volumes.intlinein : 0);

    // Is the current linein volume correct?
    int intlinein_vol = linein_getvol();
    if (intlinein_vol != intcorrect_linein_vol) my_throw("LineIn is at an incorrect level! (" + itostr(intlinein_vol) + "%)");
  }
}

long player::get_fc_segment(const long lngfc, const string & strsql_time) {
  // Fetch the the segment of format clock [lngfc] which is to be used at time [strsql_time]
  string strsql = "SELECT lngfc_seg FROM tblfc_seg WHERE lngfc=" + ltostr(lngfc) + " AND time '" + strsql_time + "' BETWEEN dtmstart AND dtmend ORDER BY lngfc_seg DESC";
  pg_result rs = db.exec(strsql);

  // Check the number of rows returned:
  if (rs.size() == 0) my_throw("Could not find a segment in the format clock!");
  if (rs.size() > 1) log_warning("Found " + itostr(rs.size()) + " matching segments! Invalid Data! Using the newest segment.");

  // Return the segment:
  return strtoi(rs.field("lngfc_seg"));
}

void player::get_playback_events_info(playback_events_info & event_info, const int intinterrupt_promo_delay) {
  // Fetch timing info about events that will take place during playback of the current item
  // (music bed starts, music bed ends, item ends, item interrupted by a promo, etc).

  // Reset the "event info" record.
  event_info.reset();

  // If our "current" item is not loaded, that means that we need to transition to the next
  // item ASAP.
  if (!run_data.current_item.blnloaded) {
    // There is no current item, so fetch the next item ASAP.
    event_info.intnext_ms      = 0;
    event_info.intitem_ends_ms = 0;
    return;
  }

  // Get info about the current item being played.

  // Fetch which XMMS session is being used to play the current item. (or if linein is being used).
  bool blnlinein_used = run_data.uses_linein(SU_CURRENT_FG);
  int intxmms_song_length_ms = -1;
  int intxmms_song_pos_ms = -1;

  if (!blnlinein_used && run_data.current_item.cat != SCAT_SILENCE) {
    // XMMS is being used to play this item.
    int intxmms_session    = run_data.get_xmms_used(SU_CURRENT_FG);
    intxmms_song_pos_ms    = xmmsc::xmms[intxmms_session].get_song_pos_ms();
    intxmms_song_length_ms = xmmsc::xmms[intxmms_session].get_song_length_ms();

    event_info.intitem_ends_ms = intxmms_song_length_ms - intxmms_song_pos_ms;
    // Does this item have a music bed?
    if (run_data.current_item.blnmusic_bed) {
      // Music bed start:
      event_info.intmusic_bed_starts_ms = run_data.current_item.music_bed.intstart_ms - intxmms_song_pos_ms;
      // Music bed end:
      event_info.intmusic_bed_ends_ms = event_info.intmusic_bed_starts_ms + run_data.current_item.music_bed.intlength_ms;

      // If the item is going to end sooner than the music bed, then use the item's end instead:
      if (event_info.intitem_ends_ms < event_info.intmusic_bed_ends_ms) {
        event_info.intmusic_bed_ends_ms = event_info.intitem_ends_ms;
      }

      // Now reset fields if they were already handled
      // (eg, we don't list ms until the music bed start if it was already
      // handled).
      if (run_data.current_item.music_bed.already_handled.blnstart) event_info.intmusic_bed_starts_ms = INT_MAX;
      if (run_data.current_item.music_bed.already_handled.blnstop)  event_info.intmusic_bed_ends_ms   = INT_MAX;
    }
  }

  // Is this item going to be interrupted to play promos? (ie, item is music, the segment allows promos, and there are waiting promos)
  if (run_data.current_item.cat == SCAT_MUSIC && !run_data.next_item.blnloaded) {
    // Current item is Music. Check if there are promos to play

    // Check if there is a promo that wants to play now (and is allowed to)
    get_next_item_promo(run_data.next_item, intinterrupt_promo_delay);

    // So, is there a promo that wants to play now?
    if (run_data.next_item.blnloaded) {
      // Yes. So the current item will be interrupted.
      event_info.intpromo_interrupt_ms = intinterrupt_promo_delay;
    }
  }

  // Did the user send an RPLS? If so (and the currently playing music) then reload the playlist ASAP,
  // and possibly interrupt the current song:
  if (run_data.current_item.cat == SCAT_MUSIC &&
      !run_data.next_item.blnloaded &&
      run_data.blnforce_segment_reload) {
    // Get the next format clock item:
    get_next_item_format_clock(run_data.next_item, intinterrupt_promo_delay);

    // Is the current item inside the current playlist?
    bool blnfound = false;
    programming_element_list::iterator i = run_data.current_segment.programming_elements.begin();
    string strcurrent_item_media = run_data.current_item.strmedia;
    while (i != run_data.current_segment.programming_elements.end()) {
      if (strcurrent_item_media == i->strmedia) {
        blnfound = true;
        break;
      }
      i++;
    }
    if (blnfound) {
      // Current item is within the new playlist. ie, no need to transition immediately to the
      // next item. If the next item is a music item then discard it now, so that any further
      // RPLS commands can be processed before the current item ends:
      if (run_data.next_item.cat == SCAT_MUSIC)
        run_data.next_item.reset();
    }
    else {
      // Next item is not within the new playlist. ie, we need to transition immediately to the
      // next item:
      event_info.intrpls_interrupt_ms = intinterrupt_promo_delay;
    }
  }

  // Interrupt the current item (regardless of type) when the hour changes:
  // -> Here we calculate the # of milliseconds between now and the start of
  //    the next hour:

  // - How many ms into the current hour?
  timeval tvnow;
  gettimeofday(&tvnow, NULL);
  int ms_into_hour = ((tvnow.tv_sec % (60*60)) * 1000) + (tvnow.tv_usec / 1000);
  // - ms until the start of the next segment?
  event_info.inthour_change_interrupt_ms = (60*60*1000) - ms_into_hour;

  // If we're playing linein or silence, then check for the next item:
  if (run_data.current_item.blnloaded &&
      run_data.current_item.strmedia == "LineIn" || run_data.current_item.cat == SCAT_SILENCE) {
    // Is the next item loaded?
    if (!run_data.next_item.blnloaded) {
      // So check if there is another item to be played:
      // How long since we last checked?
      static datetime dtmlast_check = datetime_error;
      datetime dtmnow = now();
      if (dtmlast_check > dtmnow) dtmlast_check = datetime_error; // Clock changed
      // As soon as the minute changes:
      if (dtmlast_check/60 != dtmnow/60) {
        dtmlast_check = dtmnow;
        // Attempt to fetch the next item.
        try {
          get_next_item(run_data.next_item, intinterrupt_promo_delay);
        } catch(...) {};
      }
    }

    // If the details match the current item (linein, silence, etc), then
    // reset the info now:
    if (run_data.next_item.blnloaded) {
      if (run_data.current_item.cat      == run_data.next_item.cat &&
          run_data.current_item.strmedia == run_data.next_item.strmedia) {
        // Yep. So we're still playing linein/silence. Hasn't changed
        // Reset the info about the next item:
        run_data.next_item.reset();
      }
    }

    // If we have the next item, then setup the end time of the current item:
    if (run_data.next_item.blnloaded) {
      // Linein/Silence ends immediately if there is another item
      event_info.intitem_ends_ms = 0;
    }
  }

  // Find out when the "next" event is going to take place. This is simply the smallest
  // "ms" figure that has already been found.
  event_info.intnext_ms = event_info.intitem_ends_ms;

  // Define a macro which sets A to B if B is lower.
  #define MY_SET_MIN(A,B) if (A > B) A = B
  MY_SET_MIN(event_info.intnext_ms, event_info.intmusic_bed_starts_ms);
  MY_SET_MIN(event_info.intnext_ms, event_info.intmusic_bed_ends_ms);
  MY_SET_MIN(event_info.intnext_ms, event_info.intpromo_interrupt_ms);
  MY_SET_MIN(event_info.intnext_ms, event_info.intrpls_interrupt_ms);
  MY_SET_MIN(event_info.intnext_ms, event_info.inthour_change_interrupt_ms);
  #undef MY_SET_MIN
}

int player::get_pe_vol(const string & strpe_vol) {
  // Fetch actual volume to use, based on item's "strvol" or "music_bed.strvol" settings.

  // Return music volume?
  if (strpe_vol == "MUSIC") {
    return store_status.volumes.intmusic;
  }
  // Return promo/announcement volume?
  else if (strpe_vol == "PROMO") {
    return store_status.volumes.intannounce;
  }
  // Return a % of announcement volume?
  else if (isint(strpe_vol)) {
    int intvol = strtoi(strpe_vol);
    if (intvol < 0 || intvol > 100) my_throw("Invalid volume %! - " + itostr(intvol));
    return (store_status.volumes.intannounce*intvol)/100;
  }
  // Invalid string:
  else  my_throw("Invalid Programming Element volume: \"" + strpe_vol + "\"");
}

void player::mark_promo_complete(const long lngtz_slot) {
  // This is an internal function used only by playback_transition. It is called when
  // a promo plays, to log to the database that it has played.
  string strsql = "UPDATE tblSchedule_TZ_Slot SET bitScheduled = " + itostr(ADVERT_PLAYED) +
                           ", dtmPlayedAtDate = " + psql_date + ", dtmPlayedAtTime = " + psql_time +
                           ", bitplayed = '1' "
                           " WHERE (lngTZ_Slot=" + itostr(lngtz_slot) + ")";

  db.exec(strsql);
}

// Helper function for player::log_mp_status_to_db():
void write_tblplayeroutput(pg_conn_exec & conn, const string strMessage, const string strMessageDescr) {
  // Write an entry to tblplayeroutput.
  try {
    // Build the query
    string strSQL = "INSERT INTO tblplayeroutput (strmessage, strmsgdesc, dtmtime) VALUES (" + psql_str(strMessage) + ", " + psql_str(strMessageDescr) + ", " + psql_time + ")";
    // Run the query
    conn.exec(strSQL);
  } catch_exceptions;
}

void player::log_mp_status_to_db(const sound_usage sound_usage) {
  try {
    const string strXMMS_Status = "mp_status";

    // Start a database transaction:
    pg_transaction T(db);

    // Delete all of the mp_Status records
    string strSQL = "DELETE FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strXMMS_Status);
    T.exec(strSQL);

    // Fetch info about the current or next item?
    programming_element * pe = NULL;
    switch(sound_usage) {
      case SU_CURRENT_FG:
        pe = &run_data.current_item;
        break;
      case SU_NEXT_FG:
        pe = &run_data.next_item;
        break;
      default: LOGIC_ERROR; // Should never run!
    }

    // Some variables we want to populate & write to tblplayeroutput:
    string strmp_status_playing;     // A description of what is currently playing
    string strmp_status_time;        // How long the current item has been playing.
    int    intmp_status_volume = -1; // Current volume (percent)
    string strmusic_source = "";     // "xmms", "linein", or "silence". Needed for the Wizard status update logic.

    // Generate a descriptive string for how long the current item has been playing:
    {
      datetime dtmtime_descr = now() - pe->dtmstarted + make_time(0,0,0);
      strmp_status_time = format_datetime(dtmtime_descr, "%H:%M:%S");
    }

    // Is the current item empty (ie, silence)?
    if (pe->cat == SCAT_SILENCE) {
      strmp_status_playing = "N/A - N/A";
      intmp_status_volume = 0;
      strmusic_source = "N/A";
    }
    // LineIn?
    else if (run_data.uses_linein(sound_usage)) {
      strmp_status_playing = "LineIn - external music";
      intmp_status_volume = linein_getvol();
      strmusic_source = "linein";
    }
    // Otherwise XMMS:
    else {
      // Current item is played via an XMMS session
      // - The next line will thrown an exception if this assumption is incorrect.
      int intsession = run_data.get_xmms_used(sound_usage);
      strmp_status_playing = get_short_filename(xmmsc::xmms[intsession].get_song_file_path()) + " - " + xmmsc::xmms[intsession].get_song_title();
      intmp_status_volume = xmmsc::xmms[intsession].getvol();
      strmusic_source = "xmms";
    }

    // Now log this data to the database:
    write_tblplayeroutput(T, "Playing: "      + strmp_status_playing, strXMMS_Status);
    write_tblplayeroutput(T, "Time: "         + strmp_status_time, strXMMS_Status);
    write_tblplayeroutput(T, "Left volume: "  + itostr(intmp_status_volume), strXMMS_Status);
    write_tblplayeroutput(T, "Right volume: " + itostr(intmp_status_volume), strXMMS_Status);

    // No problems, so commit the database transaction:
    T.commit();

    // Also update tblliveinfo:
    write_liveinfo_setting(db, "Music source", strmusic_source);
  } catch_exceptions;
}

int player::get_next_playback_safety_margin_ms() {
  // Fetch the current playback safety margin:
  //   How long before important playback events, the player should be ready and
  //   not run other logic which could cut into time needed for crossfading, etc.
  return config.intcrossfade_length_ms + 10000; // crossfade length + 10s.
}

