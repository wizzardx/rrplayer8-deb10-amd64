/***************************************************************************
                          player.cpp  -  The main control object
                             -------------------
    begin                : Wed Mar 16 2005
    copyright            : (C) 2005 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#include "player.h"
#include "common/testing.h"
#include "common/my_string.h"
#include "common/psql.h"
#include "config.h"
#include "common/file.h"
#include "common/system.h"
#include "common/rr_date.h"
#include "common/config_file.h"
#include "common/rr_security.h"
#include "common/dir_list.h"
#include "format_clock_test_data.h"
#include "common/xmms_controller.h"
#include "common/linein.h"
#include "common/volume_slider.h"
#include "common/my_time.h"
#include <sys/time.h>
#include "common/string_splitter.h"
#include "common/maths.h"

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

  promo.reset();  // If this item is interrupted by a promo then this var is populated  
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
        // - Do background maintenance (separate function). Events have frequencies, (sometimes desired "second" to take place at), and are prioritiesed.
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
      testing_throw;
      // Write a message to the log file and the standard out...
      string stroutput = "*** Unable to log to tblerrors - bad database connection.";
      append_file_str(strlog_file, stroutput);
      rotate_logfile(stroutput);
      cerr << stroutput << endl;
      testing_throw;
    }
  }
}

void player::init() {
  // The player init takes place here, because kdevelop breakpoints in constructors don't work.
  tzset(); // Set up C vars which indicate the timezone.

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
  load_store_status();

  // Report if the store is open or closed:
  if (store_status.blnopen) {
    log_message("Store is Open.");
  }
  else {
    log_message("Store is Closed.");
  }

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
    pg_result rs = db.exec("SELECT strmp3, stradverts, strannouncements, strspecials, strreceived, strtoday FROM tblapppaths");
    if (rs.recordcount() != 1) log_error("Invalid number of records (" + itostr(rs.recordcount()) + ") found in tblapppaths!");
    config.dirs.strmp3           = ensure_last_char(rs.field("strmp3"), '/');
    config.dirs.stradverts       = ensure_last_char(rs.field("stradverts"), '/');
    config.dirs.strannouncements = ensure_last_char(rs.field("strannouncements"), '/');
    config.dirs.strspecials      = ensure_last_char(rs.field("strspecials"), '/');
    config.dirs.strreceived      = ensure_last_char(rs.field("strreceived"), '/');
    config.dirs.strtoday         = ensure_last_char(rs.field("strtoday"), '/');
  }

  // CHECK:
  if (!dir_exists(config.dirs.strmp3))           log_error("MU directory not found: " + config.dirs.strmp3);
  if (!dir_exists(config.dirs.stradverts))       log_error("AD directory not found: " + config.dirs.stradverts);
  if (!dir_exists(config.dirs.strannouncements)) log_error("CA directory not found: " + config.dirs.strannouncements);
  if (!dir_exists(config.dirs.strspecials))      log_error("SP directory not found: " + config.dirs.strspecials);
  if (!dir_exists(config.dirs.strreceived))      log_error("Received directory not found: " + config.dirs.strreceived);
  if (!dir_exists(config.dirs.strtoday))         log_error("Today directory not found: "    + config.dirs.strtoday);

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

void player::load_store_status() {
  // Update the current store status

  // Is the store open now?
  {
    string strsql = "SELECT dtmOpeningTime, dtmClosingTime FROM tblStoreHours WHERE intDayNumber = " + itostr(weekday(now()));
    pg_result rs = db.exec(strsql);
    if (rs.recordcount() != 1) log_error("An error with table tblstorehours. Query returned " + itostr(rs.recordcount()) + " rows! (expected 1)");
    datetime dtmopen  = parse_psql_time(rs.field("dtmopeningtime"));
    datetime dtmclose = parse_psql_time(rs.field("dtmclosingtime"));

    datetime dtmtime = time();

    if (dtmopen <= dtmclose) {
      store_status.blnopen = (dtmopen <= dtmtime) && (dtmtime <= dtmclose);
    }
    else { // Weird cases, eg: Store opens at 11 PM and closes at 2 AM.
      store_status.blnopen = !((dtmtime >= dtmclose) && (dtmtime <= dtmopen));
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
  cout << strfull_path << endl;
}

void player::process_waiting_cmds() {
  undefined_throw;
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

void player::run_data::init() {
  // Run this function to reset/reinitialize playback status.
  log_message("Resetting playback.");

  // Programming elements (current item, next item):
  current_item.reset();
  next_item.reset();

  // The current Format Clock segment:
  current_segment.reset();

  // Now setup xmms sessions and tracking info:
  for (int intsession = 0; intsession < intmax_xmms; intsession++) {
    // Setup a controller for the session:
    xmms[intsession].set_session(intsession);

    // Check if the session is running:
    try {
      xmms[intsession].running(); // Throws an exception if xmms is not running.
      xmms[intsession].stop(); // Stop the XMMS session.
      xmms[intsession].setrepeat(false); // Turn off repeat
    }
    catch(...) {
      // We don't throw this as an exception. Init will still succeed. Later the
      // check_playback_status() will be called and it will throw an exception.
      log_error("XMMS session " + itostr(intsession) + " is not running!");
    }

    // Setup tracking, so we know which XMMS are currently being used for what:
    xmms_usage[intsession] = SU_UNUSED; // XMMS session is not currently being used.
  }

  // Also reset the linein volume:
  linein_usage = SU_UNUSED;
  linein_setvol(0);

  intsegment_delay = 0; // Segments are currently delayed by this number of seconds.

  waiting_promos.clear(); // List of promos waiting to play. Populated by get_next_item_promo
}

int player::run_data::get_free_xmms_session() {
  // Fetch an unused xmms session
  try {
    return get_xmms_used(SU_UNUSED); // Returns the first free XMMS session.
  } catch(...) {
    testing_throw;
    my_throw("Could not find any unused XMMS sessions!");
  }
}

void player::run_data::set_xmms_usage(const int intsession, const sound_usage sound_usage) {
  // Set the status of an XMMS session
  // A valid session number?
  if (intsession < 0 || intsession >= intmax_xmms) my_throw("Invalid XMMS session number!");

  // Are we setting the session as UNUSED, or as USED?
  if (sound_usage == SU_UNUSED) {
    // We're marking an XMMS session as unused.
    // Is the session already unused?
    if (xmms_usage[intsession] == SU_UNUSED) my_throw("XMMS session was already free, why free it again?");
    // So mark it as unused.
    xmms_usage[intsession] = SU_UNUSED;
  }
  else {
    // We're marking an XMMS session as used.
    // Is the session marked as used by something else?
    if (xmms_usage[intsession] != SU_UNUSED) my_throw("Cannot allocate an XMMS session, it is already allocated!");
    // Is this "usage" already allocated for something else?
    if (sound_usage_allocated(sound_usage)) my_throw("There is already a sound allocation for this 'usage', cannot make another allocation!");
    // Everything checks out. So mark the XMMS session as used:
    xmms_usage[intsession] = sound_usage;
  }
}

int player::run_data::get_xmms_used(const sound_usage sound_usage) {
  // Fetch which XMMS session is being used by a given "usage". eg, item fg, item bg, etc.
  // - Throws an exception if we can't find which XMMS session is being used.
  for (int intsession=0; intsession < intmax_xmms; ++intsession) {
    if (xmms_usage[intsession] == sound_usage) return intsession;
  }

  // We got this far so there is no XMMS currently being used this way.
  my_throw("I could not find the XMMS session you wanted!");
}

bool player::run_data::uses_linein(const sound_usage sound_usage) {
  // Returns true if linein is being used for the specified usage (foreground, underlying music, etc).
  return linein_usage == sound_usage;
}

bool player::run_data::sound_usage_allocated(const sound_usage sound_usage) {
  // Returns true if there is already a "sound resource" allocated for the specified usage.

  // Is LineIn allocated?
  if (linein_usage == sound_usage) return true;

  // No. Is there an XMMS session allocated?
  try {
    get_xmms_used(sound_usage);
    // No error. So there is an XMMS session reserved for this usage.
    return true;
  } catch (...) {
    // Couldn't find an XMMS session. So there aren't any LineIn sessions allocated.
    return false;
  }
}

void player::run_data::next_becomes_current() {
  // Use this to transition info, resource allocation, etc, etc from "next" to "current".

  // First free up resources used by the current item:

  // Do we have a current item? (we don't if we just started playing items).
  if (current_item.blnloaded) {
    // Does it's foreground use XMMS?
    if (current_item.strmedia == "LineIn") {
      testing_throw;
      // Does the next item use LineIn?
      if (next_item.strmedia != "LineIn") {
        testing_throw;
        // No. Set the volume to 0.
        linein_setvol(0);
      }
      // Don't do any further LineIn usage manipulation here. It was already changed, by earlier logic.
      testing_throw;
    }
    else {
      // Current item used XMMS. So Stop it's XMMS (it probably already is, but do it anyway).
      int intsession = get_xmms_used(SU_CURRENT_FG);
      xmms[intsession].stop();
      // Free it also:
      set_xmms_usage(intsession, SU_UNUSED);
    }

    // Did the current item have a music bed
    if (current_item.blnmusic_bed) {
      // Yes. Check if it is still in use:
      int intsession = -1;
      try {
        intsession = get_xmms_used(SU_CURRENT_BG);
        // If it is still used this is bad! We didn't free it earlier.
      } catch(...) { }
      if (intsession != -1) my_throw("Music Bed XMMS session should have been freed earlier!");
    }
  }

  // Change the "usage" over from "next" to "current".
  for (int intsession=0; intsession<intmax_xmms; intsession++) {
    switch(xmms_usage[intsession]) {
      case SU_UNUSED: break; // Do nothing.
      case SU_CURRENT_FG: my_throw("Logic Error!"); break; //An error. This should have been set to UNUSED earlier!
      case SU_CURRENT_BG: my_throw("Logic Error!"); break; //An error. This should have been set to UNUSED earlier!
      case SU_NEXT_FG: xmms_usage[intsession] = SU_CURRENT_FG; break; // Next FG becomes current FG.
      case SU_NEXT_BG: xmms_usage[intsession] = SU_CURRENT_BG; break; // Next BG becomes current FG.
      default: my_throw("Logic error!"); // Unknown XMMS usage for the session!
    }
  }

  // Now change next_item over to the current_item:
  current_item = next_item;

  // Next Item is now unloaded:
  next_item.reset();
}

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

void player::get_next_item(programming_element & item, const int intstarts_ms) {
  // intstarts_ms - how long from now (in ms) the item will be played.
  // Fetch the next item to be played, after the current one. Checks for announcements (promos) to play
  // format clock details, music, linein, etc, etc. Also sets a flag which determines if a crossfade
  // must take place when transitioning to the next item. When we reach the store closing hour, the next
  // item is automatically the "Silence" category (overriding anything else that might want to play now)
  // Also here must come logic for when a) repeat runs out before the segment end, and b) when some time of
  // the next segement is used up by accident (push slots forwards by up to 6 mins, reclaim space by using up music time).

  // Reset "next_item"
  item.reset();

  // Are we within store hours?
  if (!store_status.blnopen) {
testing_throw;
    // No. The next item is silence.
    item.cat = SCAT_SILENCE;
    item.blnloaded = true;
    // Also reset out "segments delayed by" factor:
    run_data.intsegment_delay=0;
  }

  if (!item.blnloaded) {
    // Inside store hours. Any promos?
    get_next_item_promo(item, intstarts_ms);
  }

  if (!item.blnloaded) {
    // No promos. Use Format Clocks to determine the next item.
    get_next_item_format_clock(item, intstarts_ms);
  }

  // Check if we found something to play:
  if (!item.blnloaded) my_throw("Could not find the next item!");
}

// Functions called by get_next_item():
void player::get_next_item_promo(programming_element & item, const int intstarts_ms) {
  // Are there any promos waiting to be returned?
  if (run_data.waiting_promos.size() != 0) {
    // Yes: Return the promo and then leave this function
    item = run_data.waiting_promos[0];
    run_data.waiting_promos.pop_front();
    return;
  }

  // No promos waiting to be returned.

  // Timing variables:
  static datetime dtmlast_now         = datetime_error; // Used to check for system clock changes
  static datetime dtmlast_run         = datetime_error; // The last time the function's main logic ran.
  static datetime dtmlast_promo_batch = datetime_error; // The last time a promo batch was returned.

  // Check if the system clock was set back in time since the last time:
  datetime dtmnow = now();
  if (dtmnow < dtmlast_now) {
    testing_throw;
    log_message("System clock change detected, recallibrating function timing...");
    dtmlast_run         = datetime_error;
    dtmlast_promo_batch = datetime_error;
  }

  // Check that enough time has passed since the last time this function was called:
  if (dtmlast_run != datetime_error && dtmnow - dtmlast_run < 30) return;

  // Now remember the last time we ran:
  dtmlast_run = dtmnow;

  // Now check that the minimum time has passed since the last announcement batch

  // Check if any ads can play now (ie, in a regular batch), or only adverts which have a specific "forced" time
  // (these could end up in "batches" also, but they take precedence over the artificial forced waits between
  // batches
  bool blnAdBatchesAllowedNow = run_data.current_segment.blnloaded &&
                                run_data.current_segment.blnpromos &&
                                (dtmnow >= dtmlast_promo_batch + 60 * config.intmin_mins_between_batches);

  // Correct announcements that were previously interrupted during playback...
  correct_waiting_promos();

  // Declare the list of items to be played. This list is used mainly for
  // Checking that ads to play do not have the same mp3 or catagory.
  TWaitingAnnouncements AnnounceList;
  TWaitingAnnouncements AnnounceMissed_SameVoice; // These are the announcements missed because their

  string psql_EarliestTime = time_to_psql(time()+(intstarts_ms/1000)-(60*config.intmins_to_miss_promos_after));

  // This query is changed in version 6.14 - ad batches are restricted to certain intervals, but adverts forced to
  // play at specific times ignore these intervals and will play as close to their playback times as possible

  // Version 6.14.3 - If necessary, check the the related tblprerec_item's lifespan...
  string strSQL = "SELECT"
                 " tblSchedule_TZ_Slot.lngTZ_Slot, tblSched.strFilename, lower(tblSched.strProductCat) AS strProductCat,"
                 " tblSched.strPlayAtPercent, tblSchedule_TZ_Slot.dtmDay, tblSlot_Assign.dtmStart, tblSlot_Assign.dtmEnd,"
                 " tblSched.strPriorityOriginal, tblSched.strPriorityConverted, tblSchedule_TZ_Slot.bitScheduled,"
                 " tblSchedule_TZ_Slot.bitPlayed, tblSchedule_TZ_Slot.dtmForcePlayAt,"
                 " lower(tblsched.strAnnCode) AS strAnnCode,"
                 " lower(tblsched.strprerec_mediaref) AS strprerec_mediaref," // 6.14.3 - The 2 fields related to PAYB
                 " tblSched.bitcheck_prerec_lifespan "
                 "FROM tblschedule_tz_slot "
                 "INNER JOIN tblSlot_Assign USING (lngAssign) "
                 "INNER JOIN tblSched ON tblSchedule_TZ_Slot.lngSched = tblSched.lngSchedule "
                 "WHERE"
                 "  tblSchedule_TZ_Slot.dtmDay = date '" + format_datetime(now() + intstarts_ms/1000, "%F")  + "'" + " AND "
                 "  tblSchedule_TZ_Slot.bitScheduled = " + itostr(ADVERT_SNS_LOADED) + " AND "
                 "("
                     "("
                       "(tblSchedule_TZ_Slot.dtmForcePlayAt >= " + psql_EarliestTime + ") AND "
                       "(tblSchedule_TZ_Slot.dtmForcePlayAt <= " + psql_time + ")"
                     ")";

  // The query above only asks for "forced" playback times. Now if they are allowed now, then also include an
  // "OR" which will include the regular, un-forced times.
  if (blnAdBatchesAllowedNow) {
    strSQL += " OR "
                   "("
                     "(tblSchedule_TZ_Slot.dtmForcePlayAt IS NULL) AND"
                     "(tblSlot_Assign.dtmStart >= " + psql_EarliestTime + ") AND "
                     "(tblSlot_Assign.dtmStart <= " + psql_time + ")"
                   ")";
  }

  // Now close the "WHERE" block and append the rest of the query.
  strSQL += ") ORDER BY tblSched.strPriorityConverted, tblSlot_Assign.dtmStart, tblSchedule_TZ_Slot.lngTZ_Slot";

  // - Player v6.15 - Now the announcement priority code (CA=1,SP=2,AD=3) actually has an effect on the announcement
  // playback priority (ORDER BY tblSched.strPriorityConverted)
  pg_result RS = db.exec(strSQL);

  // 6.14: This loop is now where announcement limiting takes place
  while (!RS.eof() && AnnounceList.size() < (unsigned) config.intmax_promos_per_batch) {
    string strdbPos, scPriority, scPriorityConv, scFileName, tmplngTZslot, strProductCat, strPlayAtPercent, strAnnCode;
    datetime dtmTime;

    strdbPos = strProductCat = scPriority = scPriorityConv = scFileName
             = strPlayAtPercent = strAnnCode = "";

    strdbPos         = RS.field("lngTZ_Slot", "");
    strProductCat    = RS.field("strProductCat", "");

    dtmTime          = parse_psql_time(RS.field("dtmForcePlayAt", RS.field("dtmStart", "").c_str()));

    // Check: Do we set the "Forced time advert" flag?
    bool blnForcedTime = !RS.field_is_null("dtmForcePlayAt");

    scPriority       = RS.field("strPriorityOriginal", "");
    scPriorityConv   = RS.field("strPriorityConverted", "");
    scFileName       = lcase(RS.field("strFileName", ""));
    strPlayAtPercent = RS.field("strPlayAtPercent", "");
    strAnnCode       = RS.field("strAnnCode", "");

    // v6.14.3 - PAYB stuff:
    string strPrerecMediaRef = lcase(RS.field("strprerec_mediaref", ""));
    bool blnCheckPrerecLifespan = (RS.field("bitcheck_prerec_lifespan", "0") == "1");

    // Interpret strPlayAtPercent
    if (isint(strPlayAtPercent)) {
      // Clip the value from 0 to 100
      if (strtoi(strPlayAtPercent) > 100) {
        testing_throw;
        strPlayAtPercent="100";
      }
      else if (strtoi(strPlayAtPercent) < 0) {
        testing_throw;
        strPlayAtPercent = "0";
      }
    }
    else {
      testing_throw;
      // Convert playback volume percentage to upper case
      strPlayAtPercent = ucase(strPlayAtPercent);

      if (strPlayAtPercent != "MUS" && strPlayAtPercent != "ADV") {
        testing_throw;
        strPlayAtPercent = "100";
      }
    }

    // Get the path of the announcement that wants to play now, it's actual filename at that path, and whether the file exists
    // on the system...
    string strFilePath = "";
    string strFilePrefix = lcase(substr(scFileName, 0, 2));

    if (strFilePrefix == "ca") {
      testing_throw;
      strFilePath = config.dirs.strannouncements; // Announcements
    }
    else if (strFilePrefix == "sp") {
      strFilePath = config.dirs.strspecials;  // Specials
    } else if (strFilePrefix == "ad") {
      testing_throw;
      strFilePath = config.dirs.stradverts; // Adverts
    }
    else {
      testing_throw;
      log_error ("Advert filename " + scFileName + " has an unknown prefix " + strFilePrefix);
      strFilePath = config.dirs.strmp3; // Default to the music folder
    }

    string strActualFileName = "";   // This is the filename of a matching file (matching meaning there
                                     // is a case-non-sensitive match of filenames

    bool blnSkipItem = false; // Set to true if the announcement is to be skipped

    if (!file_existsi(strFilePath, scFileName, strActualFileName)) {
      // MP3 not found. Is there an encrypted version of the file instead?
      if (!file_existsi(strFilePath, scFileName + ".rrcrypt", strActualFileName)) {
        // Nope, there isn't an encrypted version either.
        log_error("Could not find announcement MP3: " + strFilePath + scFileName);
        blnSkipItem = true;
      }
    }

    // v6.14.3 - PAYB stuff:
    if (blnCheckPrerecLifespan) {
      if (strPrerecMediaRef == "") {
        testing_throw;
        // The bit for checking the lifespan is set, but the media reference field is empty
        log_error("tblsched.bitcheck_prerec_lifespan set to 1, but tblsched.strprerec_mediaref is empty!");
        blnSkipItem = true;
      }
      else {
        // strprerec_mediaref and bitcheck_prerec_lifespan are set. Retrieve lifespan and global expiry date info from
        // the related tblprerec_item record
        string psql_PrerecMediaRef = psql_str(lcase(strPrerecMediaRef));
        strSQL = "SELECT intglobalexp, intlifespan FROM tblprerec_item WHERE lower(strmediaref) = " + psql_PrerecMediaRef;

        pg_result rsPrerecItem = db.exec(strSQL);
        // Check the results of the query.
        if (rsPrerecItem.recordcount() != 1) {
          // We expected to find 1 matching record, but a different number was found
          log_error(itostr(rsPrerecItem.recordcount()) + " prerecorded items match media reference " + strPrerecMediaRef + ". Cannot play " + scFileName);
          blnSkipItem = true;
        }
        else {
          testing_throw;
          // We found 1 matching record, and found it. Now check the current date against the listed global expiry date, and
          // the current lifespan, of the prerecorded item.
          int intglobalexp = strtoi(rsPrerecItem.field("intglobalexp", "-1"));
          int intlifespan = strtoi(rsPrerecItem.field("intlifespan", "-1"));

          // Get today's rr date, as an integer.
          int inttoday_rrdate = get_rrdateint(date());

          // Check the global expiry date
          if ((intglobalexp != -1) && (intglobalexp < inttoday_rrdate)) {
            testing_throw;
            // Global expiry date has elapsed!
            log_error("Advert skipped because because it's global expiry date has passed: " + scFileName);
            blnSkipItem = true;
          }
          // Check the lifespan.
          else if ((intlifespan != -1) && (intlifespan < inttoday_rrdate)) {
            testing_throw;
            // Lifespan has elapsed!
            log_error("Advert skipped because the period it was purchased for has expired: " + scFileName);
            blnSkipItem = true;
          }
          testing_throw;
        }
      }
    }

    bool blnAnnouncerClash = false; // Set to true if the announcement was skipped because the
                                    // announcer is the same as the previous one listed to be played.

    // Are there any announcements already in the queue?
    if (!blnSkipItem && AnnounceList.size() > 0) {
      // Two checks: First, the same announcement cannot play twice in an announcement batch
      //             Secondly, the same announcer cannot play twice in succession

      // . Check 1: Do not allow the same file, catagory or playback instance ID to play twice in the same announcement batch.
      TWaitingAnnouncements::const_iterator item = AnnounceList.begin();
      while (!blnSkipItem && item!=AnnounceList.end()) {
        blnSkipItem = ((*item).strFileName==scFileName) || (strProductCat != "" && (*item).strProductCat==strProductCat);
        ++item;
      }

      // . Check 2: Do not allow ads by the same announcer to play twice in succession:
      if (!blnSkipItem && strAnnCode != "") {
        if (AnnounceList[AnnounceList.size()-1].strAnnCode == strAnnCode) {
          blnSkipItem = true;
          blnAnnouncerClash = true;
        }
      }
    }

    // Now store the announcement details - we will either add it to the list of "to-play" items, or
    // the skipped (because of announcer clashes) list. If at the end of processing the announcements that
    // want to play now, we have not reached the bax maximum allowed announcements, we can try very hard
    // to find a place in the 'to-play' list where skipped announcements can be played without causing two announcements
    // by the same announcer to play in succession.
    if (!blnSkipItem || blnAnnouncerClash) {
      TWaitingAnnounce Announce;

      Announce.dbPos = strtoi(strdbPos);
      Announce.strFileName = scFileName;
      Announce.strProductCat = strProductCat;
      Announce.dtmTime = dtmTime;

      Announce.blnForcedTime = blnForcedTime;
      Announce.strPriority = scPriority;
      Announce.strPlayAtPercent = strPlayAtPercent;
      Announce.strAnnCode = strAnnCode;
      Announce.strPath = strFilePath;

      if (!blnSkipItem) {
        // This announcement will be played, queue it in the list of announcements to be played
        AnnounceList.push_back(Announce);
      }
      else {
        // This announcement possibly will not play, because the announcer was the same as the last
        // announcer in the "to-play" queue. We will try later to add it to the list anyway (if the maximum
        // allowed number of announcements per batch has not yet been reached)
        AnnounceMissed_SameVoice.push_back(Announce);
      }
    }
    RS.movenext(); // Move to the next record...
  }

  // We've reached either the end of the recordset, or the limit for number of announcements to play in a single batch.

  // If we have not reached the maximum number of allowed announcements, but there were announcements skipped
  // because their announcer was the same as the previously-queued announcer, then...
  if (AnnounceList.size() < (unsigned) config.intmax_promos_per_batch &&
    AnnounceMissed_SameVoice.size() > 0) {
    testing_throw;

    // There are missed announcements (because of the same voice), but there is still space for announcements.. here comes
    // the fun algorithm for finding a home in the "to-play" list for these voices!

    // Each time that we try to fit new announcements into the "to-play" list, and are successful, the "to-play" list length will
    // grow. If the list grows, there is a chance that we can do another pass and insert more announcements with
    // clashing codes. If the announcement list size does not grow on one pass, then there is no use in trying further passes
    long lngPrevAnnListSize = -1; // Always try at least once...
    while (lngPrevAnnListSize < (signed)AnnounceList.size()) {
      testing_throw;
      lngPrevAnnListSize = AnnounceList.size(); // Now remember the current announcement list size. If this
                                                                          // length increases, then we will run this loop again.. etc, etc..

      // Try to find a spot for each "same-voice" missed announcement, but only while there is space left in this
      // announcement batch...
      TWaitingAnnouncements::iterator item = AnnounceMissed_SameVoice.begin();
      while (item!=AnnounceMissed_SameVoice.end() &&
                AnnounceList.size() < (unsigned) config.intmax_promos_per_batch) {
        testing_throw;
        // Temporarily append the skipped announcement to the end of the "to-play" list
        //    . The item will stay in the queue if a valid playlist order is found, otherwise it will be removed later..

        AnnounceList.push_back(*item);
        // -- remember the DB ID of the announcement in case we mess up and don't get the permutations correct...
        unsigned long test_dbPos = item->dbPos;

        // - Calculate the number of permutations possible with the new "to-play" length
        long lngPermutations = calc_permutations(AnnounceList.size());

        // Start the permutation-generation... loop
        bool blnValidSequenceFound = false; // set to true if a valid order for the ads is found...
        long lngPermutationNum = 1; // The number of the current permutiation out of the total possible.

        // Init variables used for generating permutations...
        long lngTransposePos = 0; // We swap two elements, the one at this pos, and the one just after.
        int intTransposeDir = 1; // After doing a swap, the next permuation will be generated by swapping two adjacent elements
                                              // - eg after swapping element 1 and 2, we swap element 2 and 3. When we reach
                                              // the end of the list, we start moving backwards again - eg, swap 9 and 10, then swap 8 and 9
                                              // - intTransposDir controls how lngTransposePos progresses.

        while (lngPermutationNum <= lngPermutations && !blnValidSequenceFound) {
          testing_throw;
          // Firstly, is this "to-play" list valid? Check that the same announcer code does not occur twice in succession.
          bool blnTestFailed=false;
          string strPrevAnnCode = AnnounceList[0].strAnnCode;

          for (long lngAnnTest=1;lngAnnTest < (signed)AnnounceList.size() && !blnTestFailed;++lngAnnTest) {
            testing_throw;
            string strAnnCode = AnnounceList[lngAnnTest].strAnnCode;
            if (strAnnCode != "" && strAnnCode==strPrevAnnCode) {
              testing_throw;
              // Same announcement code found - test failed
              blnTestFailed = true;
            }
            else {
              testing_throw;
              // Different announcer code - update the "prev" announcer code
              strPrevAnnCode = strAnnCode;
            }
          }
          blnValidSequenceFound=!blnTestFailed;

          // Was a valid announcement order found?
          if (!blnValidSequenceFound) {
            testing_throw;
            //  If it isn't, then generate the next "transposition order" permutation, by exchanging two adjacent elements
            // of the "to-play" announcement list. Thanks go to the Canadian web-page
            // "http://www.schoolnet.ca/vp-pv/amof/e_permI.htm" where some concepts behind generating permutations are
            // explained.

            // Swap two adjacent elements...
            TWaitingAnnounce TempAnn = AnnounceList[lngTransposePos];
            AnnounceList[lngTransposePos] = AnnounceList[lngTransposePos+1];
            AnnounceList[lngTransposePos+1] = TempAnn;

            // Are there more than 2 elements?
            if (AnnounceList.size() > 2) {
              testing_throw;
              // Check the current swap pos - have we reached the end of the current direction? (depends on the
              // current direction)
              if ((intTransposeDir==1 && (lngTransposePos>=(signed)AnnounceList.size()-2)) ||
                  (intTransposeDir==-1 && (lngTransposePos<=0))) {
                testing_throw;
                // Yes - Change the progress direction..
                intTransposeDir *= -1;
              }
              // Now progress the current swap position.
              lngTransposePos += intTransposeDir;
            }
          }
          // Now we go to the next attempt to find a valid announcement order...
          ++lngPermutationNum;
        }

        bool blnEraseFromMissedList = false; // This variable determines if the next missed item
                                                                    // to be checked is determined either by 1)
                                                                    // erasing from the list (which gives you the next item)
                                                                    // or 2) - incrementing the iterator to the next item.
        if (blnValidSequenceFound)  {
          testing_throw;
          // We found a valid playbackorder, and the "to-play" announcement list now
          // contains the announcement that would have otherwise been missed.
          // - so now we remove the announcement from the "missed" list.
          blnEraseFromMissedList = true;
          testing_throw;
        }
        else {
          testing_throw;
          // A valid sequence was not found...
          //If no valid permutations were found, then remove the temporarily-added announcement from the end
          // of the"to-play" queue. Check that we have the correct element by comparing the DB id with the one we retrieved
          // earlier...

          // If we retrieved the incorrect DB id then log an error message and scan the the list for the correct element to remove
          // (it was only temporarily added), and remove it from there...
          if (AnnounceList[AnnounceList.size()-1].dbPos !=  test_dbPos) {
            testing_throw;
            log_error("Error in the permutation generation code! After all the permutations, the last element should be the same as it was at the beginning!");
            // The code outside this block is buggy - but attempt to correct the "listed to play" playlist anyway.

            TWaitingAnnouncements::iterator remove_item = AnnounceList.begin();
            while (remove_item!=AnnounceList.end()) {
              testing_throw;
              if ((*remove_item).dbPos==test_dbPos) {
                testing_throw;
                // We've found a matching item to delete - remove it and get the item after...
                remove_item==AnnounceList.erase(remove_item);
                log_error("Permutation code error was corrected.. but please check the code anyway..");
                testing_throw;
              }
              else {
                testing_throw;
                // This item isn't one to delete.. go to the next one.
                ++remove_item;
                testing_throw;
              }
              testing_throw;
            }
            testing_throw;
          }
          else {
            testing_throw;
            // The correct DB id is at the end of the list - so just erase the last item...
            AnnounceList.erase(AnnounceList.end());
            testing_throw;
          }
          testing_throw;
        }

        if (blnEraseFromMissedList) {
          testing_throw;
          // Erase an item from the missed list, because a place in the announcement order was found.
          // for it.
          item = AnnounceMissed_SameVoice.erase(item);
          testing_throw;
        }
        else {
          testing_throw;
          // A place in the announcement list was not found, just jump normally to the next
          // item we will attempt to insert....
          item++;
          testing_throw;
        }
        testing_throw;
      }
      testing_throw;
    }
    testing_throw;
  }

  bool blnForcedTimeAdToPlay = false; // Set to true if there is a "forced to play at time" advert to be played.

  // Now, we have the final list of announcements to be played. Mark these announcements in the database
  // as "listed to be played", and log a message in the player log...
  TWaitingAnnouncements::const_iterator announce_item = AnnounceList.begin();
  while (announce_item!=AnnounceList.end()) {
    // Log a message to say that this announcement is enqued
    string strTime = format_datetime((*announce_item).dtmTime, "%T");
    string strDate = format_datetime(date(), "%F");
    log_message("Announcement to be played: " + (*announce_item).strFileName +
                               ", priority: " + (*announce_item).strPriority +
                               ", catagory: \"" + (*announce_item).strProductCat +
                               "\", volume: " + (*announce_item).strPlayAtPercent +
                               "%,  time: " + strTime +
                               ", date: " + strDate +
                               ", db index: " + itostr((*announce_item).dbPos));

    // Update the database also
    strSQL = "UPDATE tblSchedule_TZ_Slot SET "
               "bitScheduled = " + itostr(ADVERT_LISTED_TO_PLAY) +
               ", dtmScheduledAtDate = " + psql_date +
               ", dtmScheduledAtTime = " + psql_time +
               " WHERE lngTZ_Slot = " + itostr((*announce_item).dbPos);
    db.exec(strSQL);

    // Set a flag if this batch to be played, includes a "force to play at time" advert.
    blnForcedTimeAdToPlay = blnForcedTimeAdToPlay || (*announce_item).blnForcedTime;

    // Move to the next "to play" item..
    ++announce_item;
  }

  // Now queue the list of announcements to be played.
  if (AnnounceList.size() > 0) {
    // This means that there are ads to play.

    // Now play the waiting announcements, one after another. This is a different approach:
    // previously a continuosly called function checked the status of
    // XMMS, and when an ad finished, it wotesting_throw;uld check for the next ad, or resume music playback.
    while (AnnounceList.size() > 0) {
      // Fetch details about the current announcement in the queue
      TWaitingAnnounce Announce = AnnounceList[0];
      AnnounceList.pop_front(); // We have the ad details now, remove it from the queue

      string strActualFileName = "";   // This is the filename of a matching file (matching meaning there
                                       // is a case-non-sensitive match of filenames

      bool blnFileExists = true; // Set to false if the file is not found.

      // File exists?
      if (!file_existsi(Announce.strPath, Announce.strFileName, strActualFileName)) {
        testing_throw;
        // No.
        blnFileExists = false;
        log_error("Could not find announcement MP3: " + Announce.strPath + Announce.strFileName);
        testing_throw;
      }

      // So did we find the file?
      if (blnFileExists) {
        // Announcement MP3 found.

        // Convert old-style volume strings:
        if (Announce.strPlayAtPercent == "MUS") {
          testing_throw;
          Announce.strPlayAtPercent = "MUSIC";
          testing_throw;
        }
        else if (Announce.strPlayAtPercent == "ADV") {
          testing_throw;
          Announce.strPlayAtPercent = "PROMO";
          testing_throw;
        }

        // Queue the announcement to play...
        {
          programming_element promo;
          promo.cat       = SCAT_PROMOS;
          promo.strmedia  = Announce.strPath + Announce.strFileName;
          promo.strvol    = Announce.strPlayAtPercent;
          promo.promo.lngtz_slot = Announce.dbPos;
          promo.blnloaded = true;
          run_data.waiting_promos.push_back(promo);
        }
      }
    }

    // Only if a regular (non-time-forced) advert batch played now, reset the time when the last
    // advert batch stopped playing. This is because we ignore forced-time playback, if for eg
    // The min time between announcemnt batches is 5 minutes, then "forced-time" playbacks
    // can occur in the middle of the 5 minutes, without affecting when the next regular announcement batch
    // will play
    if (blnAdBatchesAllowedNow) {
      dtmlast_promo_batch = now(); // The last announcement of the batch just finished playing.
                                   // So we grab the current time to ensure a minimum amount of music
                                   // before the next set of announcements can play.
    }

    // Check for ads that have been meant (they were meant to play but it's been too long since the correct time.
    write_errors_for_missed_promos();

    // Now return a promo to the calling func:
    item = run_data.waiting_promos[0];
    run_data.waiting_promos.pop_front();
  }
}

void player::get_next_item_format_clock(programming_element & next_item, const int intstarts_ms) {
  // Fetch info about the "next" item to be played, according to the Format Clock.
  // Also handle all segment timing (eg: last segment played for too long, etc.

  // Fetch the "real" time when the next item will start playing:
  datetime dtmnext_starts = now() + intstarts_ms/1000;

  // Work out the current delays.

  // Do we currently have a segment loaded?
  if (run_data.current_segment.blnloaded) {
    // The next item is meant to start 1 second after the current segment ends. Is the next item going to start
    // later than this time?
    datetime dtmseg_end = run_data.current_segment.dtmstart + run_data.current_segment.intlength - 1;
    if (dtmnext_starts > dtmseg_end + 1) {
      // Yes: Increase the segment delay factor
      int intdiff = dtmnext_starts - dtmseg_end - 1; // Take out that extra second here.
      int intnew_segment_delay = run_data.intsegment_delay + intdiff;
      if (intnew_segment_delay > intmax_segment_push_back) {
        testing_throw;
        log_warning("I have to drop " + itostr(intnew_segment_delay - intmax_segment_push_back) + "s of segment playback time! I've reached my segment 'delay' limit of " + itostr(intmax_segment_push_back) + "s");
        intnew_segment_delay = intmax_segment_push_back;
      }
      log_line("Current item is going to end " + itostr(intdiff) + "s after the current segment end. Increasing segment delay factor to " + itostr(intnew_segment_delay) + "s (+" + itostr(intnew_segment_delay - run_data.intsegment_delay) + "s)");
      run_data.intsegment_delay = intnew_segment_delay;
    }
  }

  // And now get our "delayed" time, the time in the past at which we fetch format clock data
  datetime dtmdelayed = dtmnext_starts - run_data.intsegment_delay;

  log_line("Fetching Format Clock and Segment scheduled for " + format_datetime(dtmdelayed, "%T"));

  // Reset info currently in the item:
  next_item.reset(); // Reset info currently in the item.

  // Query for the Format Clock database id & segment for this time. Include the current "segment delay" factor
  long lngfc     = -1; // -1 means no Format Clock found...
  long lngfc_seg = -1; // -1 means no Format Clock segment found.
  {
    // Variables used for fetching format clock segments:
    string strfc_date              = format_datetime(dtmdelayed, "%F");
    string strfc_time_with_hour    = format_datetime(dtmdelayed, "%T");
    string strfc_time_without_hour = format_datetime(dtmdelayed, "00:%M:%S");

    // Find a format clock and segment scheduled for this time:
    {
      // We need to fetch lngfc and lngfc_seg:
      // Fetch lngfc:
      string strsql = "SELECT lngfc FROM tblfc_sched INNER JOIN tblfc_sched_day USING (lngfc_sched) "
                      "WHERE (tblfc_sched.intday = " + itostr(weekday(dtmdelayed)) + " OR tblfc_sched.intday IS NULL) AND "
                      "date '" + strfc_date + "' BETWEEN COALESCE(tblfc_sched.dtmstart, '0000-01-01') AND "
                                                        "COALESCE(tblfc_sched.dtmend, '9999-12-25') AND "
                      "time '" + strfc_time_with_hour + "' BETWEEN tblfc_sched_day.dtmstart AND tblfc_sched_day.dtmend "
                      "ORDER BY tblfc_sched.lngfc_sched DESC LIMIT 1";
      pg_result rs = db.exec(strsql);

      // How many results?
      if (rs.recordcount() <= 0) { // No format clocks scheduled.
        log_warning("No Format Clocks scheduled for this hour. Will revert to the default Format Clock.");
      } else { // User scheduled 1 or more format clocks.
        // We found a user-scheduled format clock. Fetch the segment:
        lngfc = strtol(rs.field("lngfc"));
        try {
          lngfc_seg = get_fc_segment(lngfc, strfc_time_without_hour);
        }
        catch(const my_exception & e) {
          testing_throw;
          // We failed to get a segment.
          log_warning(e.get_error());
          log_warning("Will revert to the default format clock");
          lngfc = -1;
          lngfc_seg = -1;
        }
      }
    }

    // Did we find a format clock & segment to use?
    if (lngfc == -1) {
      // No. Fetch the Default format clock
      log_message("Reverting to Default Format Clock...");
      // Check if we have the setting:
      if (config.lngdefault_format_clock <= 0) {
        // Default clock is not set!
        log_warning("Default Format Clock is not set! Reverting to default music profile...");
      }
      else {
        // Default clock is set (in tbldefs). See if it exists on the system.
        string strsql = "SELECT lngfc FROM tblfc WHERE lngfc = " + ltostr(config.lngdefault_format_clock);
        pg_result rs = db.exec(strsql);
        // Did we find 1 record?
        if (rs.recordcount() != 1) {
          // Nope
          log_warning("Could not find the Default Format clock! (lngfc=" + ltostr(config.lngdefault_format_clock) +"). I will revert to the default music profile.");
        }
        else {
          // We found the Format Clock record. Now find the current Format Clock segment
          lngfc = config.lngdefault_format_clock;
          try {
            lngfc_seg = get_fc_segment(lngfc, strfc_time_without_hour);
          }
          catch(const my_exception & e) {
            // We failed to get a segment.
            log_warning(e.get_error());
            log_warning("Will default to the default music profile");
            lngfc = -1;
            lngfc_seg = -1;
          }
        }
      }
    }
  }

  // Has the current segment changed?
  if (!run_data.current_segment.blnloaded || lngfc_seg != run_data.current_segment.lngfc_seg) {
    // Load the new segment:
    log_message("Loading new Format Clock segment (id: " + itostr(lngfc_seg) + ")");

    // A -1 lngfc_seg means load the default music profile instead
    run_data.current_segment.load_from_db(db, lngfc_seg, config.strdefault_music_source, dtmnext_starts);

    // Log some basic details about the new segment.
    log_line(run_data.current_segment.cat.strname + " segment is scheduled for " + format_datetime(run_data.current_segment.scheduled.dtmstart, "%T") + " to " + format_datetime(run_data.current_segment.scheduled.dtmend, "%T") + " (" + itostr(run_data.current_segment.scheduled.dtmend - run_data.current_segment.scheduled.dtmstart + 1) + "s)");

    // How far into the segment did we query for?
    int intdiff = dtmdelayed - run_data.current_segment.scheduled.dtmstart;

    // Is our difference negative?
    if (intdiff < 0) my_throw("Logic Error!");
    
    // Did we query part-way into the segment?
    if (intdiff > 0) {
      log_line("Currently " + itostr(intdiff) + "s into the new segment. Compensating...");
      // Try to add this difference to our current segment delay factor
      int intnew_segment_delay = run_data.intsegment_delay + intdiff;
      if (intnew_segment_delay > intmax_segment_push_back) {
        log_warning("I have to drop " + itostr(intnew_segment_delay - intmax_segment_push_back) + "s of segment playback time! I've reached my segment 'delay' limit of " + itostr(intmax_segment_push_back) + "s");
        intnew_segment_delay = intmax_segment_push_back;
      }
      if (run_data.intsegment_delay != intnew_segment_delay) {
        log_line("Increasing 'segment delay' factor to " + itostr(intnew_segment_delay) + "s (+" + itostr(intnew_segment_delay - run_data.intsegment_delay) + "s)");
        run_data.intsegment_delay = intnew_segment_delay;
        dtmdelayed = dtmnext_starts - run_data.intsegment_delay;
      }
    }

    // Update the new segment's "start" and "length" variables.
    run_data.current_segment.dtmstart  = dtmnext_starts;
    run_data.current_segment.intlength = run_data.current_segment.scheduled.dtmend - dtmdelayed + 1;

    // Reclaim "delayed by" time if this is a music segment. But leave at least 60 seconds.
    if (run_data.current_segment.cat.cat == SCAT_MUSIC && run_data.current_segment.intlength > 60) {
      int intnew_length = run_data.current_segment.intlength - run_data.intsegment_delay;
      if (intnew_length < 60) {
        testing_throw;
        intnew_length = 60;
        testing_throw;
      }
      int intdiff = run_data.current_segment.intlength - intnew_length;
      if (intdiff > 0) {
        // Hooray, we get to reclaim some space
        log_line("Reclaiming " + itostr(intdiff) + "s of 'segment delayed' time from the current (music) segment.");
        run_data.intsegment_delay -= intdiff;
        run_data.current_segment.intlength -= intdiff;
      }
    }

    // Now log how log what time the segment will start at, and for how long it will play.
    log_message("New segment will run from " + format_datetime(run_data.current_segment.dtmstart, "%T") + " to " + format_datetime(run_data.current_segment.dtmstart + run_data.current_segment.intlength - 1, "%T") + " (" + itostr(run_data.current_segment.intlength) + "s)");
  }

  // Now fetch the next item to play, from the segment:
  run_data.current_segment.get_next_item(next_item, db, config.strdefault_music_source, intstarts_ms);
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

  if (!blnlinein_used) {
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
    }
  }

  // Is this item going to be interrupted to play promos? (ie, item is music, the segment allows promos, and there are waiting promos)
  if (run_data.current_item.cat == SCAT_MUSIC) {
    // Current item is Music. Check if there are promos to play

    // Check if there is a promo that wants to play now.
    get_next_item_promo(event_info.promo, intinterrupt_promo_delay);

    // So, is there a promo that wants to play now?
    if (event_info.promo.blnloaded) {
      // Yes. So the current item will be interrupted.
      event_info.intpromo_interrupt_ms = intinterrupt_promo_delay;
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

void player::player_maintenance(const int intmax_time_ms) {
  // Do background maintenance (separate function). Events have frequencies, (sometimes desired "second" to take place at), and are prioritiesed.
  //  - Also includes resetting info about the next playback item (highest priority, every 30 seconds..., seconds: 00, 30)
//  log_line("I have up to " + itostr(intmax_time_ms/1000) + "s to do maintenance in...");
}

void player::playback_transition(playback_events_info & playback_events) {
  // Go into an intensive timing section (5 checks every second) until we're done with the playback event. Logic for
  // transitioning between items, and introducing or cutting off underlying music, etc.

  // Possible types of transitions:
  // 1) Item ending
  // 2) Music bed starts
  // 3) Music bed ends
  // 4) Promo interrupts current playback.

  struct timeval tvprev_now; // Used for checking if the system time moves backwards (eg: ntpdate)
  gettimeofday(&tvprev_now, NULL);

  // Main loop for this function. Check if there are any nearby events for this item, to wait for and handle:
  while (playback_events.intnext_ms < intnext_playback_safety_margin_ms) {
    // We have 1 or more item playback events coming up in the near future. Prepare for them.

    // Our queue is based on the present point in time. If for example it takes 5 seconds to
    // setup the queue, then the logic will treat the queue as if it is 5 seconds late.
    struct timeval tvqueue_start;
    gettimeofday(&tvqueue_start, NULL);

    // Declare a list of events to be processed:
    transition_event_list events;
    events.clear();

    bool blncrossfade = false; // Set to true if the transition being processed involves a crossfade.
                               // This is important for knowing when to queue music bed events that
                               // will take place before the current item ends (next_becomes_curret).

    int intnext_becomes_current_ms = -1; // Will be set to the exact time in the playlist when the
                                         // next item becomes the current item. This is important
                                         // for being able to time music events

    // Current item going to end soon?
    if (playback_events.intitem_ends_ms < intnext_playback_safety_margin_ms) {
      // Yes.

      // At this point, if the previous (current) item was a promo, we mark it as complete now:
      if (run_data.current_item.blnloaded && run_data.current_item.promo.lngtz_slot != -1) {
        mark_promo_complete(run_data.current_item.promo.lngtz_slot);
      }

      // Queue a transition over to the next item:

      // Fetch the next item now.
      bool blnsegment_change = false; // We also check if the segment changes
      {
        long lngfc_seg_before = run_data.current_segment.lngfc_seg;
        get_next_item(run_data.next_item, playback_events.intitem_ends_ms);
        blnsegment_change = lngfc_seg_before != run_data.current_segment.lngfc_seg;
      }

      // Are crossfades allowed now?
      blncrossfade = false;
      if (run_data.current_item.blnloaded && (blnsegment_change || run_data.current_segment.blncrossfading)) {
        // Yes. Are we in one of the following situations? :
        // * Imaging -> Music
        // * Music   -> Music
        // * Music   -> Sweepers
        // * Music   -> Links
        // * Links   -> Music
        // * Sweeper -> Music
        blncrossfade = (run_data.current_item.cat == SCAT_IMAGING  && run_data.next_item.cat == SCAT_MUSIC) ||
                       (run_data.current_item.cat == SCAT_MUSIC    && run_data.next_item.cat == SCAT_MUSIC) ||
                       (run_data.current_item.cat == SCAT_MUSIC    && run_data.next_item.cat == SCAT_SWEEPERS) ||
                       (run_data.current_item.cat == SCAT_MUSIC    && run_data.next_item.cat == SCAT_LINKS) ||
                       (run_data.current_item.cat == SCAT_LINKS    && run_data.next_item.cat == SCAT_MUSIC) ||
                       (run_data.current_item.cat == SCAT_SWEEPERS && run_data.next_item.cat == SCAT_MUSIC);

        // We can't crossfade when going between LineIn and LineIn:
        if (run_data.current_item.strmedia == "LineIn" && run_data.next_item.strmedia == "LineIn") {
          testing_throw;
          blncrossfade = false;
        }
      }

      // So, do we crossfade between this item and the next?
      if (blncrossfade) {
        // Yes:
        // - This means that for a period of time, two items will be playing together.
        // - When we crossfade music & non-music, only the music volume is shifted. The
        //   non-music item's volume remains constant.

        // Is the current item music?
        if (run_data.current_item.cat == SCAT_MUSIC) {
          // Queue:
          //  1) A volume slide from 100% to 0
          queue_volslide(events, "current", 100, 0, playback_events.intitem_ends_ms - intcrossfade_length_ms, intcrossfade_length_ms);
        }

        // And transition in the next item:

        // Queue:
        // 1) "setup_next"
        queue_event(events, "setup_next", playback_events.intitem_ends_ms - intcrossfade_length_ms + 1);
        // 2) A fade-in (if music)
        if (run_data.next_item.cat == SCAT_MUSIC) {
          queue_volslide(events, "next", 0, 100, playback_events.intitem_ends_ms - intcrossfade_length_ms + 2, intcrossfade_length_ms);
        }
        // 3) "next_play",
        queue_event(events, "next_play", playback_events.intitem_ends_ms - intcrossfade_length_ms + 3);

        // 4) "next_becomes_current"
        queue_event(events, "next_becomes_current", playback_events.intitem_ends_ms + 3);
        intnext_becomes_current_ms = playback_events.intitem_ends_ms+3;
      }
      else {
        // No: Current item will end without a volume fade.
        // If we don't have a current item, (or we do have a current item, and it is not music)
        // and the next item is music, then we fade in the music:
        if ((!run_data.current_item.blnloaded || run_data.current_item.cat != SCAT_MUSIC) && run_data.next_item.cat == SCAT_MUSIC) {
          // We fade in the next item
          // Queue:

          //  1) "setup_next"
          queue_event(events, "setup_next", playback_events.intitem_ends_ms + 1);

          if (run_data.next_item.strmedia != "LineIn") {
            // Next item plays through XMMS:
            //  2) "setvol_next 0" (Only if XMMS)
            queue_event(events, "setvol_next 0", playback_events.intitem_ends_ms + 2);
            //  3) "next_play" (ony if XMMS)
            queue_event(events, "next_play", playback_events.intitem_ends_ms + 3);
            // 3) A volume slide from current vol (if LineIn, or 0 if XMMS) to full music
            queue_volslide(events, "next", 0, 100, playback_events.intitem_ends_ms + 4, intcrossfade_length_ms);
          }
          else {
            // Next item plays through Linein
            testing_throw;
            // 3) A volume slide from current vol (if LineIn, or 0 if XMMS) to full music
            if (linein_getvol() == 0) {
              // If linein is 0, we slide it up to full
              queue_volslide(events, "next", 0, 100, playback_events.intitem_ends_ms + 4, intcrossfade_length_ms);
            }
            else {
              // If linein is not 0, we set it to full immediately.
              queue_event(events, "setvol_next 100", playback_events.intitem_ends_ms + 4);
            }
          }
          //  4) "next_becomes_current" (transition is over)
          queue_event(events, "next_becomes_current", playback_events.intitem_ends_ms + 5 + intcrossfade_length_ms);
          intnext_becomes_current_ms = playback_events.intitem_ends_ms + 5 + intcrossfade_length_ms;
        }
        else {
          // Just queue the next item to play when this one ends.
          // Queue:
          //  1) "setup_next" (Only if XMMS).
          queue_event(events, "setup_next", playback_events.intitem_ends_ms);
          //  2) "setvol_next 100" (100% of the volume it is listed to play at)
          queue_event(events, "setvol_next 100", playback_events.intitem_ends_ms+1);
          //  2) "next_play" (Only if XMMS)
          queue_event(events, "next_play", playback_events.intitem_ends_ms+2);
          //  3) "next_becomes_current" (transition is over)
          queue_event(events, "next_becomes_current", playback_events.intitem_ends_ms+3);
          intnext_becomes_current_ms = playback_events.intitem_ends_ms+3;
        }
      }

      // Queue a "stop" for the current item's music bed if it has one
      if (run_data.current_item.blnloaded && run_data.current_item.blnmusic_bed) {
        queue_event(events, "current_music_bed_stop", intnext_becomes_current_ms-1);
      }

    }

    // Current item music bed going to start soon?
    if (playback_events.intmusic_bed_starts_ms < intnext_playback_safety_margin_ms) {
      // Yes. Queue a background music bed "start"
      // - But only if it wasn't previously queued!
      if (!run_data.current_item.music_bed.already_handled.blnstart) {
        queue_event(events, "current_music_bed_start", playback_events.intmusic_bed_starts_ms);
      }
    }

    // Current item music bed going to end soon?
    if (playback_events.intmusic_bed_ends_ms < intnext_playback_safety_margin_ms) {
      // Yes. Queue a background music bed "end"
      // - But only if it wasn't previously queued!
      testing_throw;
      if (!run_data.current_item.music_bed.already_handled.blnstop) {
        queue_event(events, "current_music_bed_end", playback_events.intmusic_bed_ends_ms);
      }
    }

    // Current item going to be interrupted by a promo sometime soon?
    // Don't do this if the next item is already loaded!
    if (!run_data.next_item.blnloaded  && playback_events.intpromo_interrupt_ms < intnext_playback_safety_margin_ms) {
      // Yes. Queue a music -> promo transition.

      // Fetch the next item from the events info:
      if (!playback_events.promo.blnloaded) my_throw("Logic Error!"); // This should have been loaded earlier
      run_data.next_item = playback_events.promo;
      playback_events.promo.reset(); // Now clear the promo data from the events info record.

      // Queue a fade-out for the current item:
      queue_volslide(events, "current", 100, 0, playback_events.intpromo_interrupt_ms, intcrossfade_length_ms);
      // Queue: setup an xmms for the next item.
      queue_event(events, "setup_next", playback_events.intpromo_interrupt_ms + intcrossfade_length_ms + 1);
      // Queue: next item play
      queue_event(events, "next_play", playback_events.intpromo_interrupt_ms + intcrossfade_length_ms + 2);
      // Queue: next item -> current item.
      queue_event(events, "next_becomes_current", playback_events.intpromo_interrupt_ms + intcrossfade_length_ms + 3);
      intnext_becomes_current_ms = playback_events.intpromo_interrupt_ms + intcrossfade_length_ms + 3;
    }

    // Now sort the queue.
    sort (events.begin(), events.end(), transition_event_less_than);

    // These are used to track the volumes set by "setvol_next" and "setvol_current". Needed so we know what
    // volume to set when we process "current_music_bed_start" and "next_music_bed_start" commands.
    int intvol_current = 100;
    int intvol_next    = 100;

    // Now we have our queue of things to do in the near future. Start a loop where we go through these
    // things.
    transition_event_list::const_iterator current_event = events.begin();
    while (current_event != events.end()) {
      // Fetch the current time:
      timeval tvnow;
      gettimeofday(&tvnow, NULL);

      // Check that the previous recorded time is less than or equal to the current time:
      if (tvprev_now.tv_sec < tvprev_now.tv_sec || (tvprev_now.tv_sec == tvprev_now.tv_sec && tvprev_now.tv_usec < tvprev_now.tv_usec)) {
        testing_throw;
        // System time has moved backwards! Possibly an ntpdate...
        // Calculate how far back the time has moved back.
        timeval tvdiff = tvprev_now;
        tvdiff.tv_sec  -= tvnow.tv_sec;
        tvdiff.tv_usec -= tvnow.tv_usec;
        normalise_timeval(tvdiff);
        log_warning("Detected: System clock moved back " + itostr(tvdiff.tv_sec) +" seconds and " + itostr(tvdiff.tv_usec) + " usecs. Optimistically adjusting playback timing back by this amount.");
        tvqueue_start.tv_sec  -= tvdiff.tv_sec;
        tvqueue_start.tv_usec -= tvdiff.tv_usec;
      }
      tvprev_now = tvnow; // Setup for the next check.

      // Calculate when the next event is to be run:
      timeval tvrun_event;
      tvrun_event = tvqueue_start;
      tvrun_event.tv_usec += current_event->intrun_ms * 1000;
      normalise_timeval(tvrun_event);

      // Calculate difference in usecs:
      timeval tvdiff = tvrun_event;
      tvdiff.tv_sec  -= tvnow.tv_sec;
      tvdiff.tv_usec -= tvnow.tv_usec;
      normalise_timeval(tvdiff);

      long lngusec_diff = tvdiff.tv_sec * 1000000 + tvdiff.tv_usec;

      // If the event is still coming, then wait for it:
      if (lngusec_diff > 0) {
        usleep(lngusec_diff);
      }

      // Now handle the event:
      {
//        cout << "[" << current_event->intrun_ms << "] " << current_event->strevent << endl;
        // Fetch the main command, and any argument from the event string:
        string strcmd = "";
        string strarg = "";
        {
          // Do the splitting here.
          string_splitter event_split(current_event->strevent);

          // Check split result:
          if (event_split.count() < 1 || event_split.count() > 2) my_throw("Logic error");

          // Fetch main command, and an arg if present.
          strcmd = event_split[0];
          if (event_split.count() == 2) {
            strarg = event_split[1];
          }
        }

        // Handle the various commands:
        if (strcmd == "setup_next") {
          // Here we fetch an available XMMS session, or LineIn. If we're using XMMS then
          // also load the item into XMMS, setup the volume, etc.

          // Is the next_item loaded?
          if (!run_data.next_item.blnloaded) my_throw("Logic Error!");
          // Are  there already XMMS or LineIn sessions allocated for the next item? (background or foreground)
          if (run_data.sound_usage_allocated(SU_NEXT_FG) || run_data.sound_usage_allocated(SU_NEXT_BG)) my_throw("Logic Error!");
          // Nope. So setup either an XMMS or LineIn:
          if (run_data.next_item.strmedia == "LineIn") {
            testing_throw;
            // Next item will play through linein.
            // Is LineIn already allocated to something else?
            if (run_data.linein_usage != SU_UNUSED) log_message("LineIn is already used for something, but commandeering it anyway for the next item.");
            run_data.linein_usage = SU_NEXT_FG;
          }
          else {
            // Next item will play through XMMS.
            // Does the item exist?
            if (!file_exists(run_data.next_item.strmedia)) my_throw("File not found! " + run_data.next_item.strmedia);

            // - Fetch a free XMMS session
            int intsession = run_data.get_free_xmms_session(); // Will throw an exception if there aren't any free.
            // - Reserve the session for next item/foreground:
            run_data.set_xmms_usage(intsession, SU_NEXT_FG);
            // - Populate the XMMS session:
            run_data.xmms[intsession].playlist_clear();
            run_data.xmms[intsession].playlist_add_url(run_data.next_item.strmedia);

            // - Set XMMS volume to the next item's volume:
            run_data.xmms[intsession].setvol(get_pe_vol(run_data.next_item.strvol), false);

            // - Make sure that repeat is turned off.
            run_data.xmms[intsession].setrepeat(false);
          }
        }
        else if (strcmd == "setvol_next" || strcmd == "setvol_current") {
          // Here we set the output volume (linein or XMMS) to a % of the total volume it wants
          // to play at. Used for volume slides.

          // Setup variables to use here, depending on if we are setting the "next" or "current" volume
          programming_element * item = NULL; // Item to use for checking
          sound_usage SU_FG = SU_UNUSED; // Foreground sound usage for the item
          sound_usage SU_BG = SU_UNUSED; // Background sound usage for the item
          int * intvol      = NULL;      // Variable used for tracking the value set by this command.

          if (strcmd=="setvol_next") {
            // Next item:
            item   = &run_data.next_item;
            SU_FG  = SU_NEXT_FG;
            SU_BG  = SU_NEXT_BG;
            intvol = &intvol_next;
          }
          else {
            // Current item:
            item = &run_data.current_item;
            SU_FG = SU_CURRENT_FG;
            SU_BG = SU_CURRENT_BG;
            intvol = &intvol_next;
          }

          // - Item loaded?
          if (!item->blnloaded) my_throw("Logic Error!");
          // Fetch the percentage to use (an error will get thrown if there is a problem):
          int intpercent = strtoi(strarg);

          // Check the range:
          if (intpercent < 0 || intpercent > 100) my_throw("Invalid volume!");

          // Remember this percentage for any "start_music_bed_current" or "start_music_bed_next" commands:
          *intvol = intpercent;

          // - LineIn or XMMS?
          if (item->strmedia == "LineIn") {
            // LineIn.
            testing_throw;
            // Check: No music beds allowed with LineIn:
            if (run_data.sound_usage_allocated(SU_BG)) my_throw("Music Bed was allocated for LineIn music!");

            // Check: LineIn is allocated to this item:
            if (!run_data.uses_linein(SU_FG)) my_throw("LineIn was not allocated!");

            // Set the volume of linein appropriately:
            linein_setvol((store_status.volumes.intlinein * intpercent)/100, false);
          }
          else {
            // Set XMMS volume:
            // Fetch session used for the foreground. Will throw an exception if it isn't allocated.
            int intsession = run_data.get_xmms_used(SU_FG);
            // Set volume appropriately:
            run_data.xmms[intsession].setvol((get_pe_vol(item->strvol) * intpercent)/100, false);

            // Set volume of music bed also if it is active now:
            {
              int intsession = -1;
              try {
                intsession = run_data.get_xmms_used(SU_BG);
                testing_throw;
              } catch(...) {
                // There is no music bed XMMS session allocated at this time. Do nothing.
              }
              if (intsession != -1) {
                testing_throw;
                // We have an XMMS session for the music bed.
                // Extra check: Does the item actually have a music bed?
                if (!item->blnmusic_bed) my_throw("Logic Error!");
                run_data.xmms[intsession].setvol((get_pe_vol(item->music_bed.strvol) * intpercent)/100, false);
              }
            }
          }
        }
        else if (strcmd == "next_play") {
          // Not relevant for LineIn. If we're using XMMS, then:
          // - Tell XMMS (for the next item) to start playing
          // - Check for music bed events that will occur *during* the current fade (if any) and queue them.

          // Is the next item loaded?
          if (!run_data.next_item.blnloaded) my_throw("Logic Error!");

          // Next item can't be linein:
          if (run_data.next_item.strmedia == "LineIn") my_throw("Don't use next_play commands for LineIn!");

          // Fetch the XMMS session:
          int intsession = run_data.get_xmms_used(SU_NEXT_FG);

          // Is XMMS already playing?
          if (run_data.xmms[intsession].playing()) my_throw("Don't use next_play when XMMS is already running!");

          // Start it playing now.
          run_data.xmms[intsession].play();

          // Log that we're playing it:
          log_message("Playing [xmms " + itostr(intsession) + "]: \"" + run_data.xmms[intsession].get_song_file_path() + "\" - \"" + run_data.xmms[intsession].get_song_title() + "\"");

          // Check if there are any music bed events (for the next item) that will occur *during* the current
          // fade (ie, before we switch over completely to the next item), and queue them here.
          if (blncrossfade && run_data.next_item.blnmusic_bed) {
testing_throw;
            // Work out the current time in ms, compared to when the queue started:
            timeval tvnow;
            gettimeofday(&tvnow, NULL);
            tvnow.tv_sec  -= tvqueue_start.tv_sec;
            tvnow.tv_usec -= tvqueue_start.tv_usec;
            normalise_timeval(tvnow);
            int intnow_ms = tvnow.tv_sec * 1000 + tvnow.tv_usec / 1000; // How many ms since the queue started.

            // Queue a "next_music_bed_start" if it does start in the near future...
            if (run_data.next_item.music_bed.intstart_ms < intnext_becomes_current_ms) {
testing_throw;
              queue_event(events, "next_music_bed_start", intnow_ms + 1 + run_data.next_item.music_bed.intstart_ms);
            }

            // Queue a "next_music_bed_end" if it does end in the near future...
            if (run_data.next_item.music_bed.intstart_ms + run_data.next_item.music_bed.intlength_ms < intnext_becomes_current_ms) {
testing_throw;
              queue_event(events, "next_music_bed_end", intnow_ms + 2 + run_data.next_item.music_bed.intstart_ms + run_data.next_item.music_bed.intlength_ms);
            }

            // Now sort the list of events:
            sort(events.begin(), events.end(), transition_event_less_than);

            testing_throw; // Check if we are at the same position in the queue as before!
          }
        }
        else if (strcmd == "next_becomes_current") {
          // Current item has just ended. It is no longer interesting in terms of player timing. So
          // we the "next" item (which we have just finished transitioning into) becomes the "current" item.

          // Now switch over resource usage:
          run_data.next_becomes_current();

          // Also the local variables we're using to track volumes (for current_music_bed_start, next_music_bed_start commands):
          intvol_current = intvol_next;
          intvol_next = 100;
        }
        else if (strcmd == "current_music_bed_start" || strcmd == "next_music_bed_start") {
          // Variables we use for our logic:
          sound_usage SU_BG = SU_UNUSED;
          programming_element * pe  = NULL;
          int intvol = 0;
          // Setup variables;
          if (strcmd == "current_music_bed_start") {
            // Current item's Music Bed: Start
            SU_BG=SU_CURRENT_BG;
            pe = &run_data.current_item;
            intvol = intvol_current;
          }
          else {
            // Next item's Music Bed: Start
            testing_throw;
            SU_BG=SU_NEXT_BG;
            pe = &run_data.next_item;
            intvol = intvol_next;
          }
          // Now run our logic for starting a music bed:

          // Check if there is already an XMMS session reserved:
          bool blnfound = false;
          try {
            run_data.get_xmms_used(SU_BG); // This should throw an exception, we don't have a current background yet.
            testing_throw;
            blnfound = true;
          } catch(...) {};
          if (blnfound) my_throw("Logic Error!");

          // Fetch an available XMMS session
          int intsession = run_data.get_free_xmms_session(); // Will throw an exception if there aren't any free
          run_data.set_xmms_usage(intsession, SU_BG);

          // Setup the session
          run_data.xmms[intsession].playlist_clear();
          run_data.xmms[intsession].playlist_add_url(pe->music_bed.strmedia);
          run_data.xmms[intsession].setvol((get_pe_vol(pe->music_bed.strvol)*intvol)/100, false);
          run_data.xmms[intsession].setrepeat(false);

          // Start the session
          run_data.xmms[intsession].play();
          // Log that we're playing underlying music:
          log_message("Music Bed [xmms " + itostr(intsession) + "]: \"" + run_data.xmms[intsession].get_song_file_path() + "\" - \"" + run_data.xmms[intsession].get_song_title() + "\"");

          // Record that this event has been handled:
          pe->music_bed.already_handled.blnstart = true;
        }
        else if (strcmd == "current_music_bed_stop" || strcmd == "next_music_bed_stop") {
          // Variables we use for our logic:
          sound_usage SU_BG = SU_UNUSED;
          programming_element * pe  = NULL;
          // Setup variables;
          if (strcmd == "current_music_bed_stop") {
            // Current item's Music Bed: Start
            SU_BG=SU_CURRENT_BG;
            pe = &run_data.current_item;
          }
          else {
            // Next item's Music Bed: Start
            testing_throw;
            SU_BG=SU_NEXT_BG;
            pe = &run_data.next_item;
          }
          // Now run our logic for starting a music bed:

          // Fetch the xmms session:
          int intsession = run_data.get_xmms_used(SU_BG);

          // Stop XMMS & free the session:
          run_data.xmms[intsession].stop();
          run_data.set_xmms_usage(intsession, SU_UNUSED);

          // Record that this event has been handled:
          pe->music_bed.already_handled.blnstop = true;
        }
        else my_throw("Unknown playback transition command! \"" + strcmd + "\"");
      }
      // Now go to the next event.
      ++current_event;
    }

    // Done with the transitions. Look for any other upcoming events during playback of the current item:
    get_playback_events_info(playback_events, intcrossfade_length_ms);
    // - Remember that we could already be part-way into the next item by the time we do this (eg: crossfade transition).
    // - If we already started or stopped a music bed then don't add them again (ie, if they are too far in the past, ignore?
  }


/*


** When starting a new item, calculate how far it will overshoot the end of the current segment by.
** Apply a lot of things here.. see current_data.
* Remember to add on current_item.overshoot value to segment lag. and then afterwards do
* reclaim if this is a music segment...

      // If this is a music segment, and we're behind, then shorten the length of the music segment
    // to reclaim lost time
    testing_throw;
    if (run_data.current_segment.cat.cat == SCAT_MUSIC && inttotal_segment_push_back > 0) {
      testing_throw;
      // Reduce music segments down to a minimum of 60 seconds:
      if (run_data.current_segment.intlength > 60) {
        int intreclaim =

      }
    }


*/

}


// Used by playback_transition():
void player::queue_event(transition_event_list & events, const string & strevent, const int intwhen_ms) {
//  cout << "Queued: [" << intwhen_ms << "] " << strevent << endl;  
  // Add an event to the queue.
  // Setup the event:
  transition_event event;
  event.intrun_ms = intwhen_ms;
  event.strevent  = strevent;

  // Add it to the queue:
  events.push_back(event);
}

void player::queue_volslide(transition_event_list & events, const string & strwhich_item, const int intfrom_vol_percent, const int intto_vol_percent, const int intwhen_ms, const int intlength_ms) {
  // Queue a volume slide from intfrom_vol_percent to intto_vol_percent.
  // Check the length of the fade:
  if (intlength_ms == 0) my_throw("Fade length cannot be 0!");
  
  int intfade_pos_ms=0; // Current position in the fade
  int intlast_vol_percent=-10000; // Used so that we don't queue duplicate setvol commands.
  while (intfade_pos_ms <= intlength_ms) {
    // Build the event to run:
    transition_event event;
    int intvol_percent = intfrom_vol_percent + ((intto_vol_percent - intfrom_vol_percent) * intfade_pos_ms) / intlength_ms;

    // Only finish building & queue the event if the volume has changed since last time:
    if (intvol_percent != intlast_vol_percent) {
      string strevent ="setvol_" + strwhich_item + " " + itostr(intvol_percent);
      int intrun_ms   = intwhen_ms + intfade_pos_ms; // When to queue this event for.    

      // And queue the event:
      queue_event(events, strevent, intrun_ms);

      // Remember that we have queued a setvol to this percentage:
      intlast_vol_percent = intvol_percent;
    }
    
    // Go another 200 ms (1/5th of a second) into the fade:
    intfade_pos_ms+=200;
  }
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
