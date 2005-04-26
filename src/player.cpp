/***************************************************************************
                          player.cpp  -  The main control object
                             -------------------
    begin                : Wed Mar 16 2005
    copyright            : (C) 2005 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#include "player.h"
#include "common/exception.h"
#include <iostream>
#include "common/file.h"
#include "common/my_string.h"
#include "config.h"
#include "common/system.h"
#include "common/rr_date.h"
#include "common/testing.h"
#include "common/config_file.h"
#include "common/rr_security.h"
#include "common/dir_list.h"
#include "common/linein.h"

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
}

// A function we use with the sort() algorithm:
bool transition_event_less_than(const transition_event & e1, const transition_event & e2) {
  return e1.intrun_ms < e2.intrun_ms;
}

// Constructor:
player::player(){
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
  // Reset data used during this loop:
  run_data.init();
  
  while (true) {
    try {
      // Sleep 1 second
      sleep (1);

      // Check playback status of XMMS, LineIn, etc. Throw errors here if there is something wrong.
      check_playback_status();
      
      // Fetch info about how long it is to go until: (item ends, music bed ends, music bed starts, next event)
      playback_events_info playback_events;
      // If a promo is going to interrupt the music, we will first fade out the music, ie it will only start after
      // [intcrossfade_length_ms] milliseconds have elapsed.
      get_playback_events_info(playback_events, intcrossfade_length_ms);
      
      if (playback_events.intnext_ms > intnext_playback_safety_margin_ms) {
        // If we have enough time left (> Safety margin, or unknown), then:
        // - Do background maintenance (separate function). Has built-in timing.
        player_maintenance(playback_events.intnext_ms - intnext_playback_safety_margin_ms);
      }
      else {
        // We're close to one or more a playback events. Handle them in an intensive timing section.
        playback_transition(playback_events);
      }      
    } catch(const exception & E) {
      log_error((string)"An unexpected error occured!");
      log_error(E.what());
      log_error("Playback reset is now required.");
      run_data.init();
    }
  }
}

void player::log(const log_info & LI) {
  // Call this to write a log to the player logfile & to the schedule database.
  bool blnlog_tbllog    = false; // Do we write to tbllog?  
  bool blnlog_tblerrors = false; // Do we write to tblerrors?
  string strtype = "";           // Message severity, used when writing to log tables.
  
  switch (LI.LT) {
    case LT_LINE: break; // Means we just log to a text file
    case LT_MESSAGE: {
      blnlog_tbllog = true;
      strtype       = "LOW";
    } break;
    case LT_WARNING: case LT_ERROR: {
      blnlog_tblerrors = true;
      strtype         = "MEDIUM";
    } break;
    default: my_throw("Logic Error!"); // There should be no other types!
  }

  // Format into a string for text-based logging:
  string strmessage = format_log(LI, strstandard_log_format);
    
  // Write to clog:
  clog << strmessage << endl;
  
  // Log to file:
  string strlog_file = PLAYER_DIR + "player.log";
  append_file_str(strlog_file, strmessage);

  // And some basic month-day-based log rotation:
  rotate_logfile(strlog_file);

  // Log to tbllog:
  if (blnlog_tbllog) {
    // Writes a log to the database. (tblLog)
    if (db.ok()) {
      // Delete previous logs of the exact same description, so that the recent logs always have the
      // highest primary key... (to aid comprehension of the table contents...)
      string strsql = "DELETE FROM tblLog WHERE tblLog.strMsg = " + psql_str(LI.strdesc);
      db.exec(strsql);

      // Now insert the line (it will be the most recent record...)
      strsql = "INSERT INTO tblLog (dtmDate, dtmTime, strType, strMsg, strFrom) VALUES (" +
             psql_date + ", " + psql_time + " ," + psql_str(strtype) + """, " + psql_str(LI.strdesc) + ", " + psql_str(LI.strfunc) + ")";
      db.exec(strsql);
    }
    else {
      // Write a message to the log file and the standard out...
      string stroutput = "*** Unable to log to tbllog - bad database connection.";
      append_file_str(strlog_file, stroutput);
      rotate_logfile(strlog_file);
      cerr << stroutput << endl;
    }
  }

  // Log to tblerrors:  
  if (blnlog_tblerrors) {
    if (db.ok()) {
      // Build a query to see whether an error record must be updated or added
      string strsql = "SELECT lngErrorNumber, lngErrorOccurred FROM tblErrors"
                               " WHERE strMsg = " + psql_str(LI.strdesc) + " AND dtmDate = " + psql_date +
                               " AND strType = " + psql_str(strtype) + " AND strFrom = " + psql_str(LI.strfunc);
      pg_result rs = db.exec(strsql);

      if (rs.recordcount() == 0) {
        // No matching records found, create a new one
        strsql =
          "INSERT INTO tblErrors (dtmDate, dtmTime, strType, strMsg, strFrom, lngErrorOccurred) "
          "VALUES (" + psql_date + "," + psql_time + "," + psql_str(strtype) + "," + psql_str(LI.strdesc) + "," + psql_str(LI.strfunc) + ",1)";
        db.exec(strsql);
      }
      else {
        // This error has already occured today, update the time and occurances
        int lngErrorNumber = strtoi(rs.field("lngErrorNumber", "-1"));
        int lngErrorOccurred = strtoi(rs.field("lngErrorOccurred", "-1"));

        strsql = "UPDATE tblErrors SET dtmTime = " + psql_time +
                 ", lngErrorOccurred = " + itostr(lngErrorOccurred + 1) +
                 " WHERE lngErrorNumber = " + itostr(lngErrorNumber);
        db.exec(strsql);
      }
    }
    else {
      // Write a message to the log file and the standard out...
      string stroutput = "*** Unable to log to tblerrors - bad database connection.";
      append_file_str(strlog_file, stroutput);
      rotate_logfile(strlog_file);
      cerr << stroutput << endl;
    }
  }
}

void player::init() {
  // Log the player's version & build numbers:
  string strintro_line = (string)" Starting Player v" + VERSION;
  string strequals_line = "";
  for (unsigned i=0; i < strintro_line.length() + 1; ++i) strequals_line += '=';
  log_line(strequals_line);
  log_line(strintro_line);
  log_line(strequals_line);

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
  if (blndebug) log_warning("Player was compiled in debugging mode");
  
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

  // Also write errors for missed announcements...
  log_message("Checking for missed promos...");
  write_errors_for_missed_promos();

  // Write some player output to the database: Music profile mp3s, playlist, xmms status
/*
  log_message("Logging XMMS status...");
  log_xmms_status_to_db();
*/

  // Also init the mp3 tags:
  mp3tags.init(PLAYER_DIR + "mp3_tags.txt");

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

  // Store's current status:
  store_status.blnopen = false;
  store_status.volumes.intmusic    = -1;
  store_status.volumes.intannounce = -1;
  store_status.volumes.intlinein   = -1;
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
  config.intmins_to_miss_promos_after = strtoi(load_tbldefs("intMissUnplayedAdsAfter",  "15", "int"));
  config.intmax_promos_per_batch      = strtoi(load_tbldefs("intMaxAdsPerBatch",        "3", "int"));
  config.intmin_mins_between_batches  = strtoi(load_tbldefs("intMinTimeBetweenAdBatch", "4", "int"));

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
    if (rs.recordcount() != 1) log_error("Invalid number of records (" + itostr(rs.recordcount()) + ") found in tblapppaths!");
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
  config.strdefault_music_source = load_tbldefs("strDefaultMusicSource", config.dirs.strmp3, "str");

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
    save_tbldefs("strDefaultMusicSource", "str", config.strdefault_music_source);

    // Now correct tblapppaths.strmp3
    config.dirs.strmp3 = strcorrect_mp3_path;
    db.exec("UPDATE tblapppaths SET strmp3 = " + psql_str(config.dirs.strmp3));
  }

  // Do promos that want to play, wait for the current song to end?
  config.blnpromos_wait_for_song_end = strtobool(load_tbldefs("blnAdvertsWaitForSongEnd", "false", "bln"));

  // Format clock settings
  config.blnformat_clocks_enabled = strtobool(load_tbldefs("blnFormatClocksEnabled", "false", "bln"));

  // Only load the "default" format clock setting if Format Clocks are enabled:
  if (config.blnformat_clocks_enabled) {
    config.lngdefault_format_clock = strtoi(load_tbldefs("lngDefaultFormatClock", "-1", "lng"));
    // CHECK:
    pg_result rs = db.exec("SELECT lngfc FROM tblfc WHERE lngfc = " + itostr(config.lngdefault_format_clock));
    if (rs.recordcount() != 1) log_error("Invalid tbldefs:lngDefaultFormatClock value! Found " + itostr(rs.recordcount()) + " matching Format Clock records!");
  }
}

// Load & Save tbldefs settings:
string player::load_tbldefs(const string & strsetting, const string & strdefault, const string & strtype) {
  // Load a value of a specific setting from the database
  // Simplified version (from VB) - load the setting from the table, don't check the type
  pg_result rs = db.exec("SELECT strDataType, strDef_Val FROM tblDefs WHERE strDef = " + psql_str(strsetting));
  if (rs.recordcount() == 0) {
    // The setting was not found in the database, add it there, and return
    // the default setting value to the caller
    save_tbldefs(strsetting, strtype, strdefault);
    return strdefault;
  } else {
    // The setting was found in the database - check it's type and then load
    // it or use the default value if the entry was incorrect
    // *** - This is a simplified version - just return the string, don't check the type
    return rs.field("strDef_Val", strdefault.c_str());
  }
}

void player::save_tbldefs(const string & strsetting, const string & strtype, const string & strvalue) {
  // Simplified version (from VB) - save the setting to the table as a string, but don't check the type
  string strsql = "SELECT strDataType, strDef_Val FROM tblDefs WHERE strDef = " + psql_str(strsetting);
  pg_result rs = db.exec(strsql);

  if (rs.recordcount() > 0) {
    // The setting already exists in the database, update it
    strsql = "UPDATE tblDefs SET strDataType = " + psql_str(strtype) + ", strDef_Val = " + psql_str(strvalue) + " WHERE strDef = " + psql_str(strsetting);
    db.exec(strsql);
  }
  else {
    // ' The setting was not found, add it to the database
    strsql = "INSERT INTO tblDefs (strDef, strDataType, strDef_Val) VALUES (" + psql_str(strsetting) + ", " + psql_str(strtype) + ", " + psql_str(strvalue) + ")";
    db.exec(strsql);
  }
}

void player::load_store_status(const bool blnverbose) {
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
    // (once every 30s)
    if (dtmlast_run/30 == dtmnow/30) return;
    
    dtmlast_run = dtmnow;
  }
  
  // Update the current store status

  // Is the store open now?
  {
    string strsql = "SELECT dtmOpeningTime, dtmClosingTime FROM tblStoreHours WHERE intDayNumber = " + itostr(weekday(now()));
    pg_result rs = db.exec(strsql);
    if (rs.recordcount() != 1) log_error("An error with table tblstorehours. Query returned " + itostr(rs.recordcount()) + " rows! (expected 1)");
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
      if (rs.recordcount() != 1) log_error("Invalid number of records (" + itostr(rs.recordcount()) + ") found in tblstore!");
      // Fetch values:
      store_status.volumes.intmusic    = strtoi(rs.field("intmusicvolume", "45"));
      store_status.volumes.intannounce = strtoi(rs.field("intannvolume", "90"));

      // Adjust values:
      store_status.volumes.intmusic    += intadjust_vol;
      store_status.volumes.intannounce += store_status.volumes.intmusic;
    }

    // Fetch the linein volume:
    store_status.volumes.intlinein   = strtoi(load_tbldefs("intLineInVol", "255", "int"));

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

  if (!RS.eof()) {
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
  if (!RS.eof()) {
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
  write_liveinfo_setting("Date", strDate);
  write_liveinfo_setting("Time", strTime);
  write_liveinfo_setting("Music vol", strMusicVol);
  write_liveinfo_setting("Announce vol", strAnnouncementVol);

/*
  write_liveinfo_setting("Adjustment vol", itostr(lrint(CurrentStatus.curAdjVol)));
*/

  write_liveinfo_setting("Ads today", strNumAdsToday);
  write_liveinfo_setting("Player version", VERSION);
/*
  write_liveinfo_setting("Music profile", strProfileName);
*/
}

void player::write_liveinfo_setting(const string strname, const string strvalue) {
  // There is a tblLiveInfo table in the DB that stores the same information as
  // the liveinfo.chk file. This procedure is used to update one of these settings
  string strSQL;

  strSQL = "SELECT lngStatus FROM tblliveInfo WHERE strstatusname = " + psql_str(strname);
  pg_result RS = db.exec(strSQL);
  // Generate part of the SQL string - if strValue is empty then a NULL value
  // must be written.
  if (RS.eof())
    // A new status setting - INSERT
    strSQL = "INSERT INTO tblliveinfo (strstatusname, strstatusvalue) VALUES (" + psql_str(strname) + ", " + psql_str(strvalue) + ")";
  else
    // An existing one - UPDATE
    strSQL = "UPDATE tblliveinfo SET strstatusvalue = " + psql_str(strvalue) + " WHERE strstatusname = " + psql_str(strname);

  db.exec(strSQL);
}

void player::check_received() {
  // this procedure checks the apppaths.recieved directory for the following files to process
  // .cmd
  string FileName; // used to read file names from a folder.
  string Full_Path;

  // Go through all files in the received folder and where necessary reset their
  // Read-Only attribute
  clear_readonly_in_dir(config.dirs.strreceived);

  // Process command files
  dir_list Dir_CMD(config.dirs.strreceived, ".cmd");
  FileName = Dir_CMD.item();
  while (FileName != "") {
    // Create the full path
    Full_Path = config.dirs.strreceived + FileName;
    log_message("Processing CMD file: " + FileName);
    // Load the command file into the database
    try {
      load_cmd_into_db(Full_Path);
      process_waiting_cmds();
    } catch_exceptions;
    rm(Full_Path);

    // Next file
    FileName = Dir_CMD.item();
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

    // Run through the waiting queries
    string strSQL = "SELECT lngWaitingCMD, strCommand, strParams, dtmProcessed, bitComplete, bitError FROM tblWaitingCMD WHERE (bitComplete = '0') OR (bitComplete IS NULL)";
    pg_result rsCMD = db.exec(strSQL);

    while(!rsCMD.eof()) {
      string strCommand = ucase(rsCMD.field("strCommand", ""));
      string strParams = rsCMD.field("strParams", "");
      lngWaitingCMD = strtoi(rsCMD.field("lngWaitingCMD"));

      log_message("Processing this command: \"" + strCommand + " " + strParams + "\"");

      try {
        if (strCommand=="RPLS") {
          undefined_throw; // Profiles not yet supported in the new player
/*        
          // Added by David - 12 November 2002
          // This is a new command in player version 6.02 when this command is found, the player will instantly stop
          // playing, reload the strmp3 path (where music is expected to be found), check the current music profile,
          // rebuild the playlist from scratch, and then resume playback. This command was added so that when the Wizard
          // wants to change the current music selection, the player will instantly respond and start playing this music.
          log_message("Processing RPLS (Reload Playlist) command...");

          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");

          // 1) Reload strmp3path (and all the other paths)
          load_db_config();

          // 2) Check the volume levels.
          load_store_hours
          volZones();

          // 3) Check the current music profile, and also rebuild the playlist (whether or not the
          // music profile changed).
          CheckMusicProfile(true);

          // Change in player version 6.14 - if playback is not currently enabled (player is paused, stopped
          // or it is outside store hours, then playback is not restarted here.
          if (PlaybackEnabled()) {
            MediaPlayer_Play(); // This function is clever enough not to interrupt current music if the music is correct.
            log_message("The updated playlist is now playing.");
          }
          else {
            // The profile changed but the player is not currently active. Log a message
            log_message("The playlist was updated, but playback is not enabled (the player is currently paused, stopped, or the time is now outside of store hours)");
          }
*/          
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
      rsCMD.movenext();
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
  long lngcorrected = RS.recordcount();

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
  string psql_EarliestTime = time_to_psql(time()- (60 * config.intmins_to_miss_promos_after));

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

  while (!RS.eof()) {
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
    RS.movenext();
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

void player::log_xmms_status_to_db() {
  undefined_throw;
}

// FUNCTIONS AND ATTRIBUTES USED DURING RUN():

void player::check_playback_status() {
  // Check playback status of XMMS, LineIn, etc. Throw errors here if there is something wrong.

  // Check XMMS sessions:
  for (int intsession=0; intsession < intmax_xmms; intsession++) {
    // Check if the session is running:
    try {
      run_data.xmms[intsession].running(); // Will throw an exception if the session is not running.
    } catch (...) {
      my_throw("XMMS session " + itostr(intsession) + " is not running!");
    }

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
      default: my_throw("Logic error!"); // This should never run.
    }

    // Is the XMMS session in use?
    if (run_data.xmms_usage[intsession] == SU_UNUSED) {
      // XMMS session is not used
      // XMMS must be stopped
      if (!run_data.xmms[intsession].stopped()) my_throw("XMMS session " + itostr(intsession) + " is meant to be in a 'stopped' state!");
    }
    else {
      // XMMS session is used. Check that it's still playing
      if (!run_data.xmms[intsession].playing()) my_throw("XMMS session " + itostr(intsession) + " is meant to be playing!");
      // Check that it's playing the correct media
      if (run_data.xmms[intsession].get_song_file_path() != strplaying) my_throw("XMMS session " + itostr(intsession) + " is playing incorrect media!");
      // Check that the volume is correct
      if (run_data.xmms[intsession].getvol() != intvol) my_throw("XMMS session " + itostr(intsession) + " has an incorrect volume!");
      // Check that repeat is off.
      if (run_data.xmms[intsession].getrepeat()) my_throw("XMMS session " + itostr(intsession) + " repeat is turned on!");
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
      default: my_throw("Logic error!"); // This should never run
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
  if (rs.recordcount() == 0) my_throw("Could not find a segment in the format clock!");
  if (rs.recordcount() > 1) log_warning("Found " + itostr(rs.recordcount()) + " matching segments! Invalid Data! Using the newest segment.");

  // Return the segment:
  return strtoi(rs.field("lngfc_seg"));
}

void player::get_playback_events_info(playback_events_info & event_info, const int intinterrupt_promo_delay) {
  // Fetch timing info about events that will take place during playback of an item (assuming it is the current item).
  // (music bed starts, music bed ends, item ends, item interrupted by a promo).

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

  // Fetch which XMMS session is being used to play the current item. (or if linein is being used).
  bool blnlinein_used = run_data.uses_linein(SU_CURRENT_FG);
  int intxmms_song_length_ms = -1;
  int intxmms_song_pos_ms = -1;

  if (!blnlinein_used && run_data.current_item.cat != SCAT_SILENCE) {
    // XMMS is being used to play this item.
    int intxmms_session    = run_data.get_xmms_used(SU_CURRENT_FG);
    intxmms_song_pos_ms    = run_data.xmms[intxmms_session].get_song_pos_ms();
    intxmms_song_length_ms = run_data.xmms[intxmms_session].get_song_length_ms();

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
    }
  }

  // Is this item going to be interrupted to play promos? (ie, item is music, the segment allows promos, and there are waiting promos)
  if (run_data.current_item.cat == SCAT_MUSIC && !run_data.next_item.blnloaded) {
    // Current item is Music. Check if there are promos to play

    // Check if there is a promo that wants to play now.
    get_next_item_promo(run_data.next_item, intinterrupt_promo_delay);

    // So, is there a promo that wants to play now?
    if (run_data.next_item.blnloaded) {
      // Yes. So the current item will be interrupted.
      event_info.intpromo_interrupt_ms = intinterrupt_promo_delay;
    }
  }
  
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
        // Yep. So we're still playing linein/silence. Hasn't change.d
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

// Called by playback_transition:
void player::log_song_played(const string & strdescr) {
  string strsql = "INSERT INTO tblMusicHistory (dtmTime, strDescription) VALUES (" + psql_now + ", " + psql_str(strdescr) + ")";
  pg_result rs = db.exec(strsql);
  
  // If there are more than 100 MP3s listed then erase the oldest MP3s
  strsql = "SELECT COUNT(lngPlayedMP3) AS Counter FROM tblMusicHistory";
  rs = db.exec(strsql);

  int intCounter = 0;

  if (!rs.eof())
    intCounter = strtoi(rs.field("Counter", "0"));

  if (intCounter > 100) {
    // There are more then 100 MP3's Listed - erase the oldest
    strsql = "SELECT lngPlayedMP3 FROM tblMusicHistory ORDER BY lngPlayedMP3 LIMIT " + itostr(intCounter - 100);
    rs = db.exec(strsql);
    while (!rs.eof()) {
      strsql = "DELETE FROM tblMusicHistory WHERE lngPlayedMP3 = " + rs.field("lngPlayedMP3", "-1");
      db.exec(strsql);
      rs.movenext();
    }
  }
}

