/***************************************************************************
                          player.cpp  -  description
                             -------------------
    begin                : Sun Jun 16 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

 /*        
               
  See ../../changelog.txt for update details.

 */

using namespace std;
 
#include <time.h>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <fstream>

#include "rr_psql.h"
#include "rr_utils.h"

#include "player.h"

#include "build_num.h"
//#include "log_catagories.h"
//#include "logging.h"
#include "dir_list.h"
#include "rr_security.h" // for string_encryption
#include "player_timed_events.h" // Timed player events.
#include "string_splitter.h"

/******************************************
   Constructor
*******************************************/

player::player() {
  // Initialize the player logging object (database connection string is sent later)
  // also set the log header text.

  string strintro_line = " Starting Player v" + strPlayerVer + " (build " + itostr(GetBuildNum()) +")";
  string strequals_line = "";
  for (unsigned i=0; i < strintro_line.length() + 1; ++i) strequals_line += '=';
  log_line(strequals_line);
  log_line(strintro_line);
  log_line(strequals_line);

  Logger=NULL; // Custom player logging (ie writing to db) set later when we have a database connection...

  // Reset the object attributes
  blninit_success = false; // Set to true if the init succeeeds
  CurrentStatus.blnMediaPaused = false;
  CurrentStatus.blnMediaStopped = false;
  blnMusicLoggingNeeded = false;
  blnTerminatePlayer = false;
  strMusicSource = strPrevMusicSource="";
  lngLastAnnID=-1;
  blndo_next_announce_batch_immediately = false;

  // Reset the event object pointers.
  timCheckMusic = NULL;
  timReceivedCheck = NULL;
  timCheckWaitingCMDs = NULL;
  timOperationalCheck = NULL;
  timScheduler = NULL;
  timCheckDBConn = NULL;
  timShowPlayerRunning = NULL;
  timCheckMusicDbErr = NULL;

  // Change to the correct folder right away (use a hard-coded value because the DB is not open yet)
  chdir(player_path.c_str());

  // Quit if the player is already open
  if (ProcessInstances("player") > 1) {
    log_message("Player already loaded, terminating...");
    return; // Failure
  };

  // Log the today's RR date.
  log_message("Today's RR date: " + DateTimeToRRDate(Date()));

  // Initialise the media-playing object.
  log_message("Initializing media playing sub-system...");
  MediaPlayer.init();

  // Attempt to set a default playlist to be used, for use if there are connection errors...
  if (FileExists(GetExecDir() + "playlist.m3u")) {
    MediaPlayer.SetMusicPlaylist(GetExecDir() + "playlist.m3u");
  }
  else {
    log_warning("Could not find most recent player music playlist (" + GetExecDir() + "playlist.m3u" + ")..");
  }

  log_message("Reading instore player configuration file...");

  // Read DB connection settings from a config file, also some other config settings
  ReadPlayerConfigFile();

  tzset(); // Set up C vars which indicate the timezone.
  log_message("Connecting to instore database (" + Config.MainDBParams.strDBName + " on " + Config.MainDBParams.strDBServer + ")...");

  // Get schedule database connection string.
  string strConn = pg_create_conn_str(
    Config.MainDBParams.strDBServer,
    Config.MainDBParams.strPort,
    Config.MainDBParams.strDBName,
    Config.MainDBParams.strUserName,
    Config.MainDBParams.strPassword);

  // Attempt to connect (retry until successful);

  // Create an event to be called when there is a database connection error, and a
  // connection retry is made
  timCheckMusicDbErr  = new event_check_music_db_err(*this);

  // Tell the database connection object to use this "event" object when there are
  // database connection errors.
  DB.call_event_on_conn_error(*timCheckMusicDbErr);
  DB.open(strConn);

  // Now init the player logger, and use it for further logging..
  Logger = new player_logger(DB);
  Logging.set_custom_logger(Logger);

  // Get path info from the instore DB
  log_message("Loading instore paths...");
  set_Paths();

  // Change to the player path (in case it is run from somewhere else)
  chdir(Config.AppPaths.strplayer.c_str());

  // Load additional Player configuration options from the instore database.
  log_message("Loading player config from instore database...");
  Load_ConfigFromDB();

  // And now that we are in the right path and have a database connection, initialize
  // the media-playing object.

  // Clear any media player commands (pause, stop, resume) that are
  // in the database and still waiting to execute...
  log_message("Removing waiting commands: mppa, mpst, mpre...");
  Remove_WaitingMediaPlayer_CMDs();

  // Is the store open/closed at this time of the day?
  log_message("Checking store status...");
  CurrentStatus.StoreStatus = SemiSonic();  // Update the "Semisonic" status (store open/closed)

  // Report store status
  if (CurrentStatus.StoreStatus == "OPEN")
    log_message("Store is Open.");
  else
    log_message("Store is Closed.");

  // We have a database connection now so we can buid a playlist
  // (Database needed for music MP3 path)

//  CurrentStatus.curVolume = XMMS.getvol();
//  log_message("Current XMMS volume is " + itostr((CurrentStatus.curVolume * 100) / 255) + "%");

  log_message("Initializing player volume levels...");
  volZones();                    // Get the correct volume to play at

  // Make sure the store music profile is correct at startup
  log_message("Checking music profile...");

  // Check the current profile, rebuild the playlist also.
  CheckMusicProfile(true);

  // Now start playing music (only if the store is open)
  if (CurrentStatus.StoreStatus == "OPEN") {
    MediaPlayer_Play();
  }
  else {
    log_message("Store closed so not playing music");
    // Also stop the music if it is playing
    MediaPlayer.Stop();
  }

  // Create the instore directory structure
  log_message("Creating directory structure...");
  Create_Directory_Structure();  // Create the directory structure

  // Create a "playing.chk" file
  log_message("Creating playing.chk file...");
  string FilePath = Config.AppPaths.strplayer + playing_chk;
  ofstream playing_chk(FilePath.c_str());

  if (playing_chk) {
    string strNow = format_datetime(Now(), "%yyyy-%mm-%dd %hh:%nn:%ss");
    playing_chk << "Started: " << strNow << endl;
    playing_chk.close();
  }
  else {
    log_error("Could not create " + FilePath);
  }

  // Generate liveinfo data
  log_message("Writing LiveInfo data...");
  doUpdateLiveInfo();

  // Check the received folder
  log_message("Checking the received folder...");
  Check_Received();              // Check the received folder, scan for CMD files

  // Added in version 6.06 - if the player is not busy playing ads, then check if there are
  // any ads that are marked as about to play. It can happen that ads are queued to play,
  // but the player application is stopped. The database will continue to show these ads as
  // 'waiting' but they will never play. Every time this proc runs, check for these
  // announcements and correct them
  log_message("Checking for invalid 'waiting' announcements...");
  CorrectWaitingAnnouncements();

  // Also write errors for missed announcements...
  log_message("Checking for missed announcements...");
  WriteErrorsForMissedAds();

  // Write some player output to the database: Music profile mp3s, playlist, xmms status
  log_message("Logging XMMS status...");
  LogXMMSStatusToDB();

  // Version 6.15 - The SNS loader will be maintained by a keep_alive script which is run at
  // regular intervals by a crontab.

  // Create the event objects
  timCheckMusic = new event_check_music(*this);
  timReceivedCheck = new event_check_received(*this);
  timCheckWaitingCMDs = new event_check_waiting_cmds(*this);
  timOperationalCheck = new event_operational_check(*this);
  timScheduler = new event_scheduler(*this);
  timCheckDBConn = new event_check_db(*this);
  timShowPlayerRunning = new event_player_running(*this);

  // Add all the events to the event runner.
  EventRunner.add_event(*timCheckMusic, 30);
  EventRunner.add_event(*timReceivedCheck, 10);
  EventRunner.add_event(*timCheckWaitingCMDs, 10);
  EventRunner.add_event(*timOperationalCheck, 30);
  EventRunner.add_event(*timScheduler, 30);
  EventRunner.add_event(*timCheckDBConn, 60);
  EventRunner.add_event(*timShowPlayerRunning, 60);

  // Show that the init succeeded.

  log_message("Player startup complete.");
  log_message("Player main event loop starting...");
  log_line("");

  // The init succeded. Set a flag to reflect this
  blninit_success = true;
}

/******************************************
   Destructor
*******************************************/

player::~player() {
  // Delete all the player events...
  delete timCheckMusic;
  delete timReceivedCheck;
  delete timCheckWaitingCMDs;
  delete timOperationalCheck;
  delete timScheduler;
  delete timCheckDBConn;
  delete timShowPlayerRunning;
  delete timCheckMusicDbErr;
  // Destroy the object for player logging (messages, warnings, errors)
  delete Logger;  
}


/**********************************
    Player Event Handling
***********************************/

// Check for any player events that need to run, run them, and return.
// returns false if it is time for the player to quit.

bool player::do_events() {
  try {
    if (blninit_success && !blnTerminatePlayer) {
      // Player is still running...      

      bool blnRunAgain = false; // This is set to true at the end of each loop iteration if
                                               // the loop must re-run..

      do {
        // Run events according to their regular schedules:
         EventRunner.run_events(); // Check which timed events must run, run them.

         // If the next advert batch must immediatly run:
         if (do_next_announce_batch_immediately()) {         
            log_message(" - The next announcement batch commences immediately");
            do_next_announce_batch_immediately(false); // Reset this status, it will be set back if necessary by the timScheduler event.
            if (timScheduler) { // Only run the event-handler if it isn't NULL.            
              timScheduler->run();
            }
         }

         // Continue to loop until the system no longer wants to play back-to-back announcement batches:
         blnRunAgain = do_next_announce_batch_immediately();
      }
      while (blnRunAgain);

      // And a temporary hack because the loader appears to be writing bad balues to Music source:
      WriteLiveInfoSetting("Music source", ((MediaPlayer.GetMusicType()==xmms) ? "xmms":"linein"));
      
      return !blnTerminatePlayer; // Return false if the player must now quit, true otherwise.
    } // end if
    else {
      return false; // Player did not init correctly, so do not check for events
    } // end else
  } catch_exceptions;

  // If code reaches this point there is an error..
  return false;
} // end funcition

/********************************************
    Check the database version
********************************************/

/***********************************************************
    Load and save settings from the database
************************************************************/

string player::LoadSettingFromDB(const string strSettingName, const string strDefault, const string strType) {
  // Load a value of a specific setting from the database
  // Simplified version (from VB) - load the setting from the table, don't check the type
  string strReturn = strDefault;
  try {
    pg_result RS = DB.exec("SELECT strDataType, strDef_Val FROM tblDefs WHERE strDef = " + psql_str(strSettingName));
    if (RS.recordcount() == 0) {
      // The setting was not found in the database, add it there, and return
      // the default setting value to the caller
      SaveSettingToDB(strSettingName, strType, strDefault);
      return strDefault;
    } else {
      // The setting was found in the database - check it's type and then load
      // it or use the default value if the entry was incorrect
      // *** - This is a simplified version - just return the string, don't check the type
      strReturn = RS.field("strDef_Val", strDefault.c_str());
    }
  } catch_exceptions;
  return strReturn;
}

void player::SaveSettingToDB(const string strSettingName, const string strType, const string strValue) {
  // Simplified version (from VB) - save the setting to the table as a string, but don't check the type
  try {
    string strSQL = "SELECT strDataType, strDef_Val FROM tblDefs WHERE strDef = " + psql_str(strSettingName);
    pg_result RS = DB.exec(strSQL);

    if (RS.recordcount() > 0) {
      // The setting already exists in the database, update it
      strSQL = "UPDATE tblDefs SET strDataType = " + psql_str(strType) + ", strDef_Val = " + psql_str(strValue) + " WHERE strDef = " + psql_str(strSettingName);
      DB.exec(strSQL);
    }
    else {
      // ' The setting was not found, add it to the database
      strSQL = "INSERT INTO tblDefs (strDef, strDataType, strDef_Val) VALUES (" + psql_str(strSettingName) + ", " + psql_str(strType) + ", " + psql_str(strValue) + ")";
      DB.exec(strSQL);
    }
  } catch_exceptions;
}

/**********************************************
    Load paths from the database
**********************************************/

// Sets paths for easier access later
void player::set_Paths() {
  try {
    //    With AppPaths
    string strSQL = "SELECT * FROM tblAppPaths";
    pg_result RS = DB.exec(strSQL);
    // Load values
    Config.AppPaths.strmain             = RS.field("strmain");
    Config.AppPaths.stradmin            = RS.field("stradmin");
    Config.AppPaths.strmp3              = RS.field("strmp3");
    Config.AppPaths.stradverts          = RS.field("stradverts");
    Config.AppPaths.strannouncements    = RS.field("strannouncements");
    Config.AppPaths.strspecials         = RS.field("strspecials");

    Config.AppPaths.strconfirmbroadcast = RS.field("strconfirmbroadcast");
    Config.AppPaths.stremergency        = RS.field("stremergency");
    Config.AppPaths.strerror            = RS.field("strerror");
    Config.AppPaths.strplaylist         = RS.field("strplaylist");
    Config.AppPaths.strreceived         = RS.field("strreceived");
    Config.AppPaths.strreturns          = RS.field("strreturns");
    Config.AppPaths.strschedules        = RS.field("strschedules");
    Config.AppPaths.strtemp             = RS.field("strtemp");
    Config.AppPaths.strtoday            = RS.field("strtoday");
    Config.AppPaths.strroot             = RS.field("strroot");
    Config.AppPaths.strinstoredb        = RS.field("strinstoredb");
    Config.AppPaths.stragent            = RS.field("stragent");
    Config.AppPaths.strupdater          = RS.field("strupdater");
    Config.AppPaths.strloader           = RS.field("strloader");
    Config.AppPaths.strplayer           = RS.field("strplayer");

    // New paths added in version 6.01
    // Removed in version 6.14
//    AppPaths.strmp3_alt           = RS.field("strmp3_alt");
//    AppPaths.stradverts_alt       = RS.field("stradverts_alt");
//    AppPaths.strannouncements_alt = RS.field("strannouncements_alt");
//    AppPaths.strspecials_alt      = RS.field("strspecials_alt");

    // Another useful path
    Config.AppPaths.strprofiles = RS.field("strprofiles");

    // Append slashes to the paths as necessary
    Config.AppPaths.strmain          = ensure_char_at_end(Config.AppPaths.strmain, '/');
    Config.AppPaths.stradmin         = ensure_char_at_end(Config.AppPaths.stradmin, '/');
    Config.AppPaths.strmp3           = ensure_char_at_end(Config.AppPaths.strmp3, '/');
    Config.AppPaths.stradverts       = ensure_char_at_end(Config.AppPaths.stradverts, '/');
    Config.AppPaths.strannouncements = ensure_char_at_end(Config.AppPaths.strannouncements, '/');

    Config.AppPaths.strspecials         = ensure_char_at_end(Config.AppPaths.strspecials, '/');
    Config.AppPaths.strconfirmbroadcast = ensure_char_at_end(Config.AppPaths.strconfirmbroadcast, '/');
    Config.AppPaths.stremergency        = ensure_char_at_end(Config.AppPaths.stremergency, '/');
    Config.AppPaths.strerror            = ensure_char_at_end(Config.AppPaths.strerror, '/');
    Config.AppPaths.strplaylist         = ensure_char_at_end(Config.AppPaths.strplaylist, '/');

    Config.AppPaths.strreceived  = ensure_char_at_end(Config.AppPaths.strreceived, '/');
    Config.AppPaths.strreturns   = ensure_char_at_end(Config.AppPaths.strreturns, '/');
    Config.AppPaths.strschedules = ensure_char_at_end(Config.AppPaths.strschedules, '/');
    Config.AppPaths.strtemp      = ensure_char_at_end(Config.AppPaths.strtemp, '/');
    Config.AppPaths.strtoday     = ensure_char_at_end(Config.AppPaths.strtoday, '/');
    Config.AppPaths.strroot      = ensure_char_at_end(Config.AppPaths.strroot, '/');
    Config.AppPaths.strinstoredb = ensure_char_at_end(Config.AppPaths.strinstoredb, '/');
    Config.AppPaths.stragent     = ensure_char_at_end(Config.AppPaths.stragent, '/');
    Config.AppPaths.strupdater   = ensure_char_at_end(Config.AppPaths.strupdater, '/');
    Config.AppPaths.strloader    = ensure_char_at_end(Config.AppPaths.strloader, '/');
    Config.AppPaths.strplayer    = ensure_char_at_end(Config.AppPaths.strplayer, '/');

// Commented out in version 6.14
//    // New paths added in version 6.01
//    ensure_char_at_end(&AppPaths.strmp3_alt, '/');
//    ensure_char_at_end(&AppPaths.stradverts_alt, '/');
//    ensure_char_at_end(&AppPaths.strannouncements_alt, '/');
//    ensure_char_at_end(&AppPaths.strspecials_alt, '/');

    Config.AppPaths.strprofiles = ensure_char_at_end(Config.AppPaths.strprofiles, '/');

    // Load the "default music path" to use
    Config.strdefault_music_source = LoadSettingFromDB("strDefaultMusicSource", Config.AppPaths.strmp3, "str");
    // Check tblapppaths.strmp3 - it should not be the same as tblapppaths.strprofiles
    //  - if it is then this means that the Wizard has probably set a music source (linein/cd music, etc) in a deprecated
    // way. Correct strmp3 and update the default music source setting.
    if (Config.AppPaths.strmp3 != strcorrect_mp3_path) {
      log_warning("Detected: The Wizard used a deprecated method for setting music type! Correcting...");

      // Remember the incorrect strmp3 setting as the default music source setting...
      Config.strdefault_music_source = Config.AppPaths.strmp3;
      SaveSettingToDB("strDefaultMusicSource", "str", Config.strdefault_music_source);

      // Now correct tblapppaths.strmp3
      Config.AppPaths.strmp3 = strcorrect_mp3_path;
      strSQL = "UPDATE tblapppaths SET strmp3 = " + psql_str(Config.AppPaths.strmp3);
      DB.exec(strSQL);
    }
  } catch_exceptions;
}

/*********************************************************
    Load other (non-path) database settings.
*********************************************************/

void player::Load_ConfigFromDB() {
  try {
    // Load the settings
    Config.intMinsToMissAdsAfter = strtoi(LoadSettingFromDB("intMissUnplayedAdsAfter", itostr(intDefault_MinsToMissAdsAfter), "int"));
    Config.intMaxAdsPerBatch = strtoi(LoadSettingFromDB("intMaxAdsPerBatch", itostr(intDefault_MaxAdsPerBatch), "int"));
    Config.intMinMinsBetweenAdBatches = strtoi(LoadSettingFromDB("intMinTimeBetweenAdBatch", itostr(intDefault_MinMinsBetweenAdBatches), "int"));

    // Added in player version 6.19 - Load the player's fade-in and fade-out length from the database at startup:
    int intMusicFadeOutLength = strtoi(LoadSettingFromDB("intMusicFadeOutLength", itostr(intDefault_MusicFadeOutLength), "int"));
    int intMusicFadeInLength  = strtoi(LoadSettingFromDB("intMusicFadeInLength", itostr(intDefault_MusicFadeInLength), "int"));

    MediaPlayer.SetMusicFadeOutLength(intMusicFadeOutLength);
    MediaPlayer.SetMusicFadeInLength(intMusicFadeInLength);

    // Added in player version 6.21 - Do we wait for the current song to finish before playing ads?
    // Exceptions: Linein music, and adverts with "forced" playback times.
    Config.blnAdvertsWaitForSongEnd = strtobool(LoadSettingFromDB("blnAdvertsWaitForSongEnd", "false", "bln"));

    // Check the settings.
    if (Config.intMinsToMissAdsAfter <= 0 || Config.intMinsToMissAdsAfter >= 10000) {
      // Invalid value, correct it.
      Config.intMinsToMissAdsAfter = intDefault_MinsToMissAdsAfter;
      log_error("Invalid value for setting: Minutes after which to miss unplayed ads. Defaulted to " + itostr(intDefault_MinsToMissAdsAfter));
    }

    if (Config.intMaxAdsPerBatch <= 0 || Config.intMaxAdsPerBatch > 10) {
      Config.intMaxAdsPerBatch = intDefault_MaxAdsPerBatch;
      log_error("Invalid value for setting: Maximum ads per advert batch. Defaulted to " + itostr(intDefault_MaxAdsPerBatch));
    }

    if (Config.intMinMinsBetweenAdBatches < 0 || Config.intMinMinsBetweenAdBatches > 60) {
      Config.intMinMinsBetweenAdBatches = intDefault_MinMinsBetweenAdBatches;
      log_error("Invalid value for setting: Minimum minutes between ad batches. Defaulted to " + itostr(intDefault_MinMinsBetweenAdBatches));
    }
  } catch_exceptions;
}

/********************
    Config file.
********************/

void player::ReadPlayerConfigFile() {
  // Load blank values into the config vars, so we can warn the user when default values are loaded

  // - Database connection settings
  Config.MainDBParams.strDBServer = "";
  Config.MainDBParams.strDBName   = "";
  Config.MainDBParams.strUserName = "";
  Config.MainDBParams.strPassword = "";
  Config.MainDBParams.strPort     = "";

// - As of player version 6.14, the following settings are loaded from the database,
// not the config file:
//  player_config.intMinsToMissAdsAfter = -1;
//  player_config.intMaxAdsPerBatch = -1;
//  player_config.intMinMinsBetweenAdBatches = -1;

  // Open the conf file
  string strConfPath = GetExecDir() + "player.conf";
  ifstream inFile(strConfPath.c_str());
  if (!inFile) {
    log_error("Unable to open " + strConfPath + ", using default settings");
    return;
  }

  // Read through all the lines
  string FileLine;

  char ch_FileLine[2048] = "";
  int LineCounter=0;
  while (inFile.getline(ch_FileLine, 2047)) {
    FileLine = ch_FileLine;
    FileLine = remove_ending_cr(FileLine);
    LineCounter++;   // Update the line count
    // Process the line

    if ((FileLine.length() > 0) && (FileLine.c_str()[0] != '#')) {
      string In_Identifier, In_Value; // Variables used for decoding lines

      Process_Conf_Line(FileLine, In_Identifier, In_Value);

      if(strcasecmp(In_Identifier.c_str(), "DB_Server")==0) {
        Config.MainDBParams.strDBServer = In_Value;
      }
      else if (strcasecmp(In_Identifier.c_str(), "DB_Name")==0) {
        Config.MainDBParams.strDBName = In_Value;
      }
      else if (strcasecmp(In_Identifier.c_str(), "User_Name")==0) {
        Config.MainDBParams.strUserName = In_Value;
      }
      else if (strcasecmp(In_Identifier.c_str(), "User_Password")==0) {
        // Change in player 6.15 - database login password is encrypted.
        string_encryption StringEnc(In_Value);
        
        try {
          StringEnc.decrypt(get_rr_encrypt_key(), 2);
          // Successfully decrypted the password string
          Config.MainDBParams.strPassword = In_Value;
        }
        catch (const rr_exception & E) {
          // Could not decrypt the password string.
          log_error("Error in player.conf line " + itostr(LineCounter) + ":" + FileLine +
            " - could not decrypt password. Reason: " + E.get_error());
        }
      }
      else if (strcasecmp(In_Identifier.c_str(), "DB_Port")==0) {
        Config.MainDBParams.strPort = In_Value;
      }
      else {
        // The identifier is not recognised - report an error
        log_error("Error in player.conf line " + itostr(LineCounter) + ":" + FileLine +
          " - Unrecognised identifier " + In_Identifier);
      }
    }
  }

  // Now check which configuration options were not loaded, warn the user, and load the default values.

  // - Database connection settings
  if (Config.MainDBParams.strDBServer =="") {
    Config.MainDBParams.strDBServer = strDefaultMainDBServer;
    log_message("Database server not specified, using default");
  }

  if (Config.MainDBParams.strDBName  == "") {
    Config.MainDBParams.strDBName   = strDefaultMainDBName;
    log_message("Database name not specified, using default");
  }

  if (Config.MainDBParams.strUserName == "") {
    Config.MainDBParams.strUserName = strDefaultUser;
    log_message("Database user not specified, using default");
  }

  if (Config.MainDBParams.strPassword == "") {
    Config.MainDBParams.strPassword = strDefaultPassword;
    log_message("Database password not specified, using default");
  }

  if (Config.MainDBParams.strPort == "") {
    Config.MainDBParams.strPort     = strDefaultPort;
    log_message("Database server port not specified, using default");
  }
}

/*************************************************************
    Pause, Stop and Resume media playback
*************************************************************/

void player::Media_Pause() {
  // Pause the media playback. This means that when the user hits "resume", the current MP3 must
  // continue from the position it was at.
  try {
    // Update the player status variable
    CurrentStatus.blnMediaPaused = true;

    // Pause the MediaPlayer object
    MediaPlayer.Pause();

    log_message("Media playback paused");
  } catch_exceptions;
}

void player::Media_Stop() {
  // Stop the media playback. This means that when the player is told to Resume, the
  // next announcement or music file is played.
   try {
    // Update the player status variable
    CurrentStatus.blnMediaStopped = true;

    // Stop the MediaPlayer object
    MediaPlayer.Stop();

    log_message("Media playback stopped");
  } catch_exceptions;
}

void player::Media_Resume() {
  // If the player is already playing, ignore. If the player is paused, resume the
  // original player status (ie, continue the last MP3, finish playing the announcement session)
  // If the player was stopped, then just go to the next song or announcement.

  // Change in player 6.15 - The Media_Resume() function doesn't correctly implement a
  // "Unpause" function. It just starts playing music (if not already playing), and reenables
  // player events, with no attempt to resume the advert.

  try {
    log_message("Resuming Media playback...");

    // Update the status variables - we're no longer paused or stopped
    CurrentStatus.blnMediaPaused = false;
    CurrentStatus.blnMediaStopped = false;

    if (PlaybackEnabled()) {
       MediaPlayer_Play();
    } // end if

  } catch_exceptions;
}

/****************************************************************************************
    Functions for handling player commands (stored in the database)
****************************************************************************************/

bool player::Load_CMDIntoDB(const string strFull_Path) {
  try {
    // Open the command file and start reading commands
    ifstream inFile(strFull_Path.c_str());

    if (!inFile) {
      // Unable to open file - log error and return false
      log_error("Unable to open command file " + strFull_Path);
      remove(strFull_Path.c_str());
      return false;
    }
    else {
      char ch_FileLine[2048] = "";
      if (!inFile.getline(ch_FileLine, 2047)) {
        // Couldn't read the first file line (with RAT validation code)
        log_error("Invalid command file (no lines): " + strFull_Path);
        remove(strFull_Path.c_str());
        return false;
      }

      string strFileLine = ch_FileLine;
      strFileLine = remove_ending_cr(strFileLine);
      enumRAT retRAT = RAT(strFileLine);

      bool blnEncrypt;

      if (retRAT == ratUnEncrypted) {
        blnEncrypt = false;
      }

      else if (retRAT == ratEncrypted) {
        blnEncrypt = true;
      }
      else {
        // - problem, kill this file and go to the next one
        log_error("Non RAT complaint CMD file, " + strFull_Path);
        remove(strFull_Path.c_str());
        return false;
      }

      while (inFile.getline(ch_FileLine, 2047)) {
        strFileLine = ch_FileLine;
        strFileLine = remove_ending_cr(strFileLine);
        string strUniqueID;

        // Grab the command details and write to the waiting CMD table

        // - First of all generate the unique ID to use for this command-file line
        string strSQL = "SELECT strUniqueID FROM tblWaitingCMD WHERE substr(strUniqueID, 1, 3) = 'CMD' ORDER BY strUniqueID DESC LIMIT 1";
        pg_result RS = DB.exec(strSQL);

        if (RS.eof()) {
          strUniqueID = "CMD0000000";
        }
        else {
          strUniqueID = RS.field("strUniqueID", "CMD0000000");
        }

        string strCmd = StringToUpper(substr(strFileLine, 0, 4));
        string strParams = substr(strFileLine, 4, strFileLine.length());

        // Actually this may be a command with 4 chars not 5, check for that
        if ((strCmd=="IPST") || (strCmd=="LIRE") || (strCmd=="NTRE") || (strCmd=="RSOF") || (strCmd=="STCL") || (strCmd=="STOP")) {
          strCmd = StringToUpper(substr(strFileLine, 0, 5));
          strParams = substr(strFileLine, 5, strFileLine.length());
        }

        char ch_UniqueID[10];
        sprintf(ch_UniqueID, "%07d", strtoi(substr(strUniqueID, 3, strUniqueID.length()))+1);

        strUniqueID = string("CMD") + ch_UniqueID;

        // Get the short filename (remove everything up to and including the last "/")
        string strShortFileName = "";
        strShortFileName = GetShortFileName(strFull_Path);

        // Build up the query
        strSQL = "INSERT INTO tblWaitingCMD (strUniqueID, strCommand, strParams, strCMDFileName) "
                 "VALUES (" + psql_str(strUniqueID) + ", " + psql_str(strCmd) + ", " + psql_str(strParams) +  ", " + psql_str(strShortFileName) + ")";

        DB.exec(strSQL);
      }
      return true;
    }
  } catch_exceptions;
  return false; // Return was meant to happen earlier!
}

void player::Process_WaitingCMDs() {
  // Process CMD command files that are sent to the store and stored in the database
  long lngWaitingCMD = -1; // Used for error logging
  string strSQL;

  try {
    bool blnVolZones = false;
    double volChange;
    string chDay, chZone, chTime;

    bool blnStopPlayer = false;
    bool blnRestartLinux = false;

    string psql_Time;
    string psql_Now;

    // Run through the waiting queries
    string strSQL = "SELECT lngWaitingCMD, strCommand, strParams, dtmProcessed, bitComplete, bitError FROM tblWaitingCMD WHERE (bitComplete = '0') OR (bitComplete IS NULL)";
    pg_result rsCMD = DB.exec(strSQL);

    while(!rsCMD.eof()) {
      string strCommand = StringToUpper(rsCMD.field("strCommand", ""));
      string strParams = rsCMD.field("strParams", "");
      lngWaitingCMD=strtoi(rsCMD.field("lngWaitingCMD"));

      log_message("Processing this command: \"" + strCommand + " " + strParams + "\"");

      try {

        if (strCommand == "CHMU") {
          // Change the Music volume.

          if (!IsInt(strParams)) rr_throw("Argument must be a number!");
          volChange = strtoi(strParams);

          if (volChange > 250)
            volChange = 250;

          strSQL = "UPDATE tblStore SET intMusicVolume = " + itostr(lrint(volChange));
          DB.exec(strSQL);
          log_message("CHMU: Changed music volume to " + itostr(lrint(volChange)));
          blnVolZones = true;
        }
        else if (strCommand == "CHAN") {
          // Change the Announcement volume
          if (!IsInt(strParams)) rr_throw("Argument must be a number!");
          volChange = strtoi(strParams);

          if (volChange>250)
            volChange=250;

          strSQL = "UPDATE tblStore SET intAnnVolume = " + itostr(lrint(volChange));
          DB.exec(strSQL);

          log_message("CHAN: Changed announcement volume to " + itostr(lrint(volChange)));

          blnVolZones = true;
        }
        else if (strCommand == "CADJ") {
          // Change the RR Trafic Adjustment volume
          if (strParams.length()!=6) rr_throw("Argument must be 6 characters long!");
          if (!IsInt(strParams)) rr_throw("Argument must be an integer!");

          chDay = substr(strParams, 0, 1);
          chZone = substr(strParams, 1, 2);
          if (substr(strParams, 3, 3) == "")
            volChange = 0;
          else
            volChange = strtoi(substr(strParams, 3, 3));

          strSQL = "SELECT intVolAdj, intDayNumber, lngTimeZone FROM tblVolumeZones "
                   "WHERE (tblVolumeZones.intDayNumber=" +
                   chDay +
                   ") AND (tblVolumeZones.lngTimeZone=" +
                   chZone + ")";

          pg_result RS = DB.exec(strSQL);
          if (!RS.eof()) {
            // Update the volumezone record
            strSQL = "UPDATE tblVolumeZones SET intVolAdj = " + itostr(lrint(volChange)) + " WHERE intDayNumber = " +
                     chDay + " AND lngTimeZone = " + chZone;
            DB.exec(strSQL);
          }
          else {
            // Create a new volumezone record
            string psql_StartTime = TimeToPSQL(MakeTime(strtoi(chZone)-1, 0, 0));
            string psql_EndTime = TimeToPSQL(MakeTime(strtoi(chZone)-1, 59, 59));

            strSQL = "INSERT INTO tblVolumeZones(intDayNumber,lngTimeZone,intVolAdj,dtmTimeStart,dtmTimeEnd) "
              "VALUES (" + chDay + "," + chZone + "," + itostr(lrint(volChange)) + ", " + psql_StartTime + ", " + psql_EndTime +  ")";
            DB.exec(strSQL);
          }
          log_message( "CADJ: Changed music volume entry on day " + chDay + ", zone " + chZone + " to " + itostr(lrint(volChange)));
          blnVolZones = true;
        }
        else if (strCommand=="CHLI") {
          // Change the LINE IN volume

          if (!IsInt(strParams)) rr_throw("Argument must be an integer!");
          volChange = strtoi(strParams);

          if (volChange>255)
            volChange=255;

          SaveSettingToDB("intLineInVol", "int", itostr(lrint(volChange)));
          log_message("CHLI: Changed linein volume to " + itostr(lrint(volChange)));
          blnVolZones = true;
        }
        else if (strCommand=="STOPE") {
          chDay = substr(strParams, 0, 1);
          chTime = substr(strParams, 1, 5);
          psql_Time = TimeToPSQL(MakeTime(strtoi(substr(chTime, 0, 2)), strtoi(substr(chTime, 3, 2)), 0));

          if (chDay < "1" || chDay > "7") {
            rr_throw("Command STOPE " + strParams + " is invalid - weekday is out of bounds");
          }

          strSQL = "SELECT intDayNumber FROM tblStoreHours WHERE intDayNumber = " + chDay;
          pg_result RS = DB.exec(strSQL);

          if (!RS.eof()) {
            // Update existing record
            strSQL = "UPDATE tblStoreHours SET dtmOpeningTime = " + psql_Time + " WHERE intDayNumber = " + chDay;
          }
          else {
            // Add a new record
            strSQL = "INSERT INTO tblStoreHours(dtmOpeningTime,intDayNumber) VALUES (" + psql_Time + "," + chDay + ")";
          }

          DB.exec(strSQL);
          log_message("STOPE: Changed Store Opening Time for weekday " + chDay + " to " + chTime);
        }
        else if (strCommand=="STCLS") {
          // changes the closing entry in the database
          chDay = substr(strParams, 0, 1);
          chTime = substr(strParams, 1, 5);
          psql_Time = TimeToPSQL(MakeTime(strtoi(substr(strParams, 0, 2)), strtoi(substr(strParams, 4, 2)), 0));

          if (chDay < "1" || chDay > "7") {
            rr_throw("Command STCLS " + strParams + " is invalid - weekday is out of bounds");
          }

          strSQL = "SELECT intDayNumber FROM tblStoreHours WHERE intDayNumber = " + chDay;
          pg_result RS = DB.exec(strSQL);

          if (RS.eof()) {
            // No record, add a new one
            strSQL = "INSERT INTO tblStoreHours (dtmClosingTime,intDayNumber) VALUES (" + psql_Time + "," + chDay + ")";
          }
          else {
            // Update an existing record
            strSQL = "UPDATE tblStoreHours SET dtmClosingTime = " + psql_Time + " WHERE intDayNumber = " + chDay;
          }
          DB.exec(strSQL);
          log_message("STCLS: Changed Store Closing Time for day " + chDay + " to " + chTime);
        }
        else if (strCommand=="IPSTO") {
          blnStopPlayer = true;  // Stop the Instore Player.
        }
        else if (strCommand=="LIRES" || strCommand=="NTRES") {
          blnRestartLinux = true;
        }
        else if (strCommand=="CSSP") {
          // (Change Scheduling Skip Period) Update the period in
          // which the in-store scheduling must skip time each
          // hour - Added by David (20/12/2000))
          if (strParams.length() !=12 || !IsInt(strParams)) {
            rr_throw("Invalid Change Scheduling Skip Period");
          }
          else {
            // Save the CSSP settings to the database
            SaveSettingToDB("dtmStartSchedSkipPeriod", "str", substr(strParams, 4, 4));
            SaveSettingToDB("dtmEndSchedSkipPeriod", "str", substr(strParams, 0, 4));
            SaveSettingToDB("intStartMinuteSkipPeriod", "int", substr(strParams, 8, 2));
            SaveSettingToDB("intEndMinuteSkipPeriod", "int", substr(strParams, 10, 2));
          }
        }
        else if (strCommand=="RPLS") {
          // Added by David - 12 November 2002
          // This is a new command in player version 6.02 when this command is found, the player will instantly stop
          // playing, reload the strmp3 path (where music is expected to be found), check the current music profile,
          // rebuild the playlist from scratch, and then resume playback. This command was added so that when the Wizard
          // wants to change the current music selection, the player will instantly respond and start playing this music.
          log_message("Processing RPLS (Reload Playlist) command...");

          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");

          // 1) Reload strmp3path (and all the other paths)
          set_Paths();

          // 2) Check the volume levels.
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
        }
        // Some commands added in version 6.11 - allow the user to pause, stop and resume the media playback.
        //
        else if (strCommand=="MPPA") {
          log_message("Processing MPPA (Media Player Pause) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          Media_Pause();
        }
        else if (strCommand=="MPST") {
          log_message("Processing MPST (Media Player Stop) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          Media_Stop();
        }
        else if (strCommand=="MPRE") {
          log_message("Processing MPRE (Media Player Resume) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          Media_Resume();
        }
        else if (strCommand=="RCFG") {
          // Added in version 6.14 on 05/08/2003 - the player now loads some of it's config options from the
          // database at startup. When the player reads an "RCFG" command it will reload these settings
          log_message("Processing RCFG (Reload Config) command...");
          // Log a warning if there are args
          if (strParams != "") log_warning("This command does not take arguments!");
          Load_ConfigFromDB();
        }
        else {
          // The command is unknown, report an error
          rr_throw("Unknown command " + strCommand);
        }

        strSQL = "UPDATE tblWaitingCMD SET bitComplete = '1',bitError = '0',dtmProcessed = " +
                 psql_now + " WHERE lngWaitingCMD = " + rsCMD.field("lngWaitingCMD", "-1");
        DB.exec(strSQL);
      }
      catch(const rr_exception & E) {
        log_error("Error with this command: " + strCommand + (strParams != "" ? (string(" ") + strParams + string(" ")) : "") + " - " + E.get_error());
        strSQL = "UPDATE tblWaitingCMD SET bitComplete = '1', bitError = '1', dtmProcessed = " + psql_now + " WHERE lngWaitingCMD = " + rsCMD.field("lngWaitingCMD", "-1");
        DB.exec(strSQL);
      }
      rsCMD.movenext();
    }

    // Anything else that needs to happen at the end;
    if (blnVolZones) volZones();
    if (blnStopPlayer) blnTerminatePlayer = true;
    if (blnRestartLinux) RestartLinux();
  }
  catch(...) {
    strSQL = "UPDATE tblWaitingCMD SET dtmProcessed=" + psql_now + ", bitComplete='1', bitError='1' WHERE lngWaitingCMD = " + ltostr(lngWaitingCMD);
    DB.exec(strSQL);
    throw;
  }
}

void player::Remove_WaitingMediaPlayer_CMDs() {
  // Mark all unprocessed Media Player commands (MPPA, MPST, MPRE) as complete, but with errors.
  // This function is called at the start of the player, so that if the user clicked the wizard's
  // pause, stop, or resume buttons a lot, the player won't spend time trying to catch up with these
  // clicks.
  try {
    string strSQL = "UPDATE tblWaitingCMD SET bitComplete = '1', bitError = '1', dtmProcessed = " + psql_now + " WHERE ((lower(strcommand)='mppa') OR (lower(strcommand)='mpst') OR (lower(strcommand)='mpre')) AND ((bitComplete = '0') OR (bitComplete IS NULL))";
    DB.exec(strSQL);
  } catch_exceptions;
}

/*************************************
    Create the instore paths
*************************************/

void player::Create_Directory_Structure() {
  //creates the required directory structure for the InStore Player
  try {
    mkdir(Config.AppPaths.strmain);
    mkdir(Config.AppPaths.stradmin);
    mkdir(Config.AppPaths.strmp3);
    mkdir(Config.AppPaths.stradverts);
    mkdir(Config.AppPaths.strannouncements);
    mkdir(Config.AppPaths.strspecials);

    mkdir(Config.AppPaths.strconfirmbroadcast);
    mkdir(Config.AppPaths.stremergency);

    mkdir(Config.AppPaths.strerror);
    mkdir(Config.AppPaths.strplaylist);
    mkdir(Config.AppPaths.strreceived);

    mkdir(Config.AppPaths.strreturns);

    mkdir(Config.AppPaths.strschedules);
    mkdir(Config.AppPaths.strtemp);
    mkdir(Config.AppPaths.strtoday);
    //  mkdir(AppPaths.strroot);    // Don't try to create the root path
    mkdir(Config.AppPaths.strinstoredb);
    mkdir(Config.AppPaths.stragent);
    mkdir(Config.AppPaths.strupdater);
    mkdir(Config.AppPaths.strloader);
    mkdir(Config.AppPaths.strplayer);

//    Removed in player version 6.14
//    // New paths in player version 6.01
//    mkdir(AppPaths.strmp3_alt);

    mkdir(Config.AppPaths.strprofiles);
    // These other directories will be mounted from the old NT D: drive
    // - stradverts_alt, strannouncements_alt, strspecials_alt
  } catch_exceptions;
}

/*************************************************************************************
    Return true if playback is currently enabled (music and adverts)
**************************************************************************************/
bool player::PlaybackEnabled() {
  try {
    return SemiSonic()=="OPEN" && !CurrentStatus.blnMediaPaused && !CurrentStatus.blnMediaStopped;
  } catch_exceptions;
  return false;
}

/***********************
    Music profiles
***********************/
bool player::CheckMusicProfile(const bool blnForcePlaylistRebuild) {
  // Search for which profile should be active now, and compare it to the current one. Change over
  // to the new profile (and music playlist) if necessary. Return True if the profile changed and the playlist was regenerated.
  // If blnForcePlaylistRebuild==true then rebuild the playlist whether or not the profile changed
  // -> The default value for blnForcePlaylistRebuild is False

  try {
    // Some variables
    string strNewMusicSource = "";

    // Change in player version 6.14 - The playlist can now be changed by this procedure if
    // player playback is disabled (PlaybackEnabled()) - however, the calling function
    // should still check PlaybackEnabled() to check if MediaPlayer.Play() is allowed..
    if (strMusicSource == "") strMusicSource = Config.strdefault_music_source;// If source = nothing, source is default music source..

    long lngprofile_highest = -1; // Database index of the highest tblmusicprofiles.lngprofile found so far.
                                                 // - The highest found lngprofile

    string strProfileName = "";  // Name of the profile to use... (empty = Default mp3 repository)

    // ========================================================================
    // Check #1 - Check tblmusicprofile fields (strstartday, dtmstarttime, strendday, dtmendtime)
    // ========================================================================

    bool blnprofile_found = false; // Set to true when a matching music profile is found

    string strSQL = "SELECT * FROM tblMusicProfiles WHERE bitEnabled = '1' ORDER BY lngprofile DESC";
    pg_result RS = DB.exec(strSQL);

    while ((!RS.eof()) && (!blnprofile_found)) {
      // Set some flags for this iteration:
      bool blnskip_profile = false; // Set to true if for some reason this profile must be skipped (error, not applicable, etc)

      // Get the details
      strProfileName        = RS.field("strProfileName", "");
      string strStartDay    = RS.field("strStartDay", "");
      DateTime dtmStartTime = parse_psql_time(RS.field("dtmStartTime", "0001-01-01 00:00:00"));
      string strEndDay      = RS.field("strEndDay", "");
      DateTime dtmEndTime   = parse_psql_time(RS.field("dtmEndTime",   "0001-01-01 23:59:59"));
      string strMusic       = RS.field("strMusic", "");
      long lngprofile       = strtol(RS.field("lngprofile", "-1"));

      int intStartWeekDay = -1;
      int intEndWeekDay = -1;
      bool blnStartIsWeekDay = false;
      bool blnEndIsWeekDay = false;
      bool blnOnStartDay = false;
      bool blnOnEndDay = false;

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
        if (IsDate(strStartDay)) {
          blnStartIsWeekDay = false;
        } // if (IsDate(strStartDay))
        else {
          // It's not a date. Is it a week-day?
          strStartDay = StringToLower(strStartDay);
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
        if (IsDate(strEndDay)) {
          blnEndIsWeekDay = false;
        } // if (IsDate(strEndDay))
        else {
          strEndDay = StringToLower(strEndDay);
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
          int intWeekDay = Weekday(Date());

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
          DateTime dtmStartDay, dtmEndDay;
          dtmStartDay = ParseDateString(strStartDay);
          dtmEndDay = ParseDateString(strEndDay);

          int intNowYear, intNowMonth, intNowDay,
              intStartYear, intStartMonth, intStartDay,
              intEndYear, intEndMonth, intEndDay;

          GetDateParts(Now(), intNowYear, intNowMonth, intNowDay);
          GetDateParts(dtmStartDay, intStartYear, intStartMonth, intStartDay);
          GetDateParts(dtmEndDay, intEndYear, intEndMonth, intEndDay);

          int intYearDiff = intStartYear - intNowYear;
          intStartYear -= intYearDiff;
          intEndYear -= intYearDiff;

          dtmStartDay = MakeDate(intStartYear, intStartMonth, intStartDay);
          dtmEndDay  = MakeDate(intEndYear, intEndMonth, intEndDay);

          // A small correction: IN case this has moved the start day after today, but actually the start
          // date must be last year and this is before the end date.
          if (dtmStartDay > Now()) {
            intStartYear--;
            intEndYear--;
            dtmStartDay = MakeDate(intStartYear, intStartMonth, intStartDay);
            dtmEndDay   = MakeDate(intEndYear, intEndMonth, intEndDay);
          } // if (dtmStartDay > Now())

          // Now check the dates
          blnDayCorrect = ((Date() >= dtmStartDay) && (Date() <= dtmEndDay));

          blnOnStartDay = (Date() == dtmStartDay);
          blnOnEndDay   = (Date() == dtmEndDay);
        } // else

        if (blnDayCorrect) {
          // The day is correct. Check the times!
          if (blnOnStartDay) {
            if (Time() < dtmStartTime) {
              // Before the start time. Try the next profile
              blnskip_profile = true;
            } // if (Time() < dtmStartTime)
            else if (blnOnEndDay) {
              if (Time() > dtmEndTime) {
                // After the end time. Try the next profile
                blnskip_profile = true;
              } // if (Time() > dtmEndTime)
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
      RS.movenext();
    } // while ((!RS.eof()) && (!blnprofile_found))

    // ========================================================================
    // Check #2 - Check tblmusicprofile_date and tblmusicprofile_timezone for any profiles
    //                   that have been *scheduled* to play in this hour.
    // ========================================================================

    // Fetch the related tlktimezone record index...
    long lngtimezone = -1; // The index of the timezone record...

    // tlktimezone time fields do not contain seconds. ie no matching records will be returned when
    // the current time is hh:59:ss - where ss is any second after 00.
    // - Therefore - fetch the current time, and truncate the seconds.
    DateTime dtmApproxTime = (Time() / 60) * 60; // DateTime vars are stored as # seconds.
    string psqlApproxTime  = TimeToPSQL(dtmApproxTime);

    strSQL = "SELECT lngtimezone FROM tlktimezone WHERE dtmtzfrom <= " + psqlApproxTime + " AND " + psqlApproxTime + " <= dtmtzto";
    RS = DB.exec(strSQL);

    if (RS.recordcount() != 1) {
      // Expected 1 record to be found!
      log_error("Error with tlktimezone table data. This query produced " +itostr(RS.recordcount()) + " records where 1 was expected: " + strSQL);
      lngtimezone = -1;
    } // if (RS.recordcount() != 1)
    else {
      // 1 record was found.
      lngtimezone = strtol(RS.field("lngtimezone"));
    } // else

    // Now search for the most recently-added profile that was scheduled to play in this hour(timezone) :
    strSQL = "SELECT tblmusicprofiles.lngprofile, tblmusicprofiles.strprofilename, tblmusicprofiles.strmusic "
                   "FROM tblmusicprofiles "
                   "INNER JOIN tblmusicprofile_date ON tblmusicprofiles.lngprofile = tblmusicprofile_date.lngprofile "
                   "INNER JOIN tblmusicprofile_timezone ON tblmusicprofile_date.lngprofile_date = tblmusicprofile_timezone.lngprofile_date "
                   "WHERE tblmusicprofiles.bitenabled='1' AND "
                   "tblmusicprofile_date.dtmday = " + psql_date + " AND "
                   "tblmusicprofile_timezone.lngtimezone = " + ltostr(lngtimezone) +
                   " ORDER BY tblmusicprofile_timezone.lngprofile_timezone DESC "
                   "LIMIT 1";
    RS = DB.exec(strSQL);

    // Was a tblmusicprofiles record retrieved for this hour(timezone)?
    if (!RS.eof()) {
      // A tblmusicprofiles record was retrieved for this hour.
      long lngprofile = strtol(RS.field("lngprofile","-1"));
      strProfileName = RS.field("strprofilename", "");
      string strMusic = RS.field("strmusic", "");

      // Compare this profile with the highest lngprofile found so far (ie, any retrieved from Check #1)
      if (lngprofile > lngprofile_highest) {
        // The lngprofile of this profile is higher than that of the profile found earlier (if one was found).
        // - So use this profile instead, because it was created more recently.
        strNewMusicSource = strMusic;
        lngprofile_highest = lngprofile; // This is the highest "matching" lngprofile value found so far...
        blnprofile_found = true;
      } // if (lngprofile > lngprofile_highest)
    } // if (!RS.eof())

    if (strNewMusicSource == "") {
      // This means that no matching profile was found.
      strNewMusicSource = Config.strdefault_music_source;
      strProfileName = "";
    } // if (strNewMusicSource == "")

    if (blnForcePlaylistRebuild || strNewMusicSource != strPrevMusicSource) {
      // The profile has changed, or a playlist change is forced, then change now.
      strPrevMusicSource = strNewMusicSource;
      strMusicSource = strNewMusicSource;
      // - Create a new playlist and start it playing
      log_message("Changing to music profile \"" + (strProfileName=="" ? "default" : strProfileName) + "\": " + strNewMusicSource);

      // Build a new playlist, and set it as the playlist that will be played by the MediaPlayer
      bool blnCreatePlaylistSuccess = false;
      try {
        CreateRandomPlaylist();
        blnCreatePlaylistSuccess = true;
      } catch_exceptions;
      
      // If there was an error, see if we can fall back to default music...
      if (!blnCreatePlaylistSuccess && (strMusicSource != Config.strdefault_music_source)) {
        // There was an error creating the random playlist... attempt to fall back to the default music location...
        log_message("Error creating playlist, attempting to use default music location: " + Config.strdefault_music_source);
        strMusicSource = Config.strdefault_music_source;

        try {
          CreateRandomPlaylist();
          blnCreatePlaylistSuccess = true;          
        } catch_exceptions;
      }
      // If there was an error, see if we can fall back to the default mp3 repository directory...
      if (!blnCreatePlaylistSuccess && (strMusicSource != Config.AppPaths.strmp3)) {
        // There was an error creating the random playlist... attempt to fall back to the default music location...
        log_message("Error creating playlist, attempting to use default mp3 repository: " + Config.AppPaths.strmp3);
        strMusicSource = Config.AppPaths.strmp3;
        try {
          CreateRandomPlaylist();
          blnCreatePlaylistSuccess = true;
        } catch_exceptions;
      }
      // If the music playlist has still not been successfully played, then log this as a critical error..
      if (!blnCreatePlaylistSuccess) {
        log_error("Could not create a music playlist! Music will not play correctly!");
        // Update the liveinfo table profile setting:
        WriteLiveInfoSetting("Music profile", "ERROR");
        return false;
      }

      // Do a quick update of the liveinfo table to reflect the current profile.
      WriteLiveInfoSetting("Music profile", (strProfileName=="") ? "Default profile" : strProfileName);

      return true; // The profile changed - return true - however we still need to
                         // check PlaybackEnabled() and call MediaPlayer.PlayMusic();
    } // if (blnForcePlaylistRebuild || strNewMusicSource != strPrevMusicSource)
  } catch_exceptions;
  return false; // If the code reaches this point, the profile did not change
} // end function

bool player::check_short_weekday(const string & strShortWeekDay, int & intWeekDay) {
  // Return true if strDay is mon, tue, wed, etc, and populate intWeekday with the weekday number - 1, 2, 3, etc.
  bool blnresult = false;
  try {
    intWeekDay = -1;

    // Search for the string
    const string ShortWeekDays[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun" };
    int i = 0; // Index into the ShortWeekDays array.

    for (i=0; i < 7 && ShortWeekDays[i] != strShortWeekDay; ++i);

    // Was the string found?
    if (i < 7) {
      // Yes. Return the index (+1) and a success value
      intWeekDay = i+1;
      blnresult = true;
    }
  } catch_exceptions;
  return blnresult;
}

/**********************
    Music Playlist
***********************/

void player::CreateRandomPlaylist() {
  // Log a message to say a random playlist is being created
  log_message("Creating random playlist...");

  bool blnUseM3U = false;
  // New addition - global string strMusicSource contains
  // the M3U or folder to generate playlists from. If
  // strMusicSource is invalid or "" then use AppPaths.MP3 by
  // default.

  typedef vector<string> string_vector;
  string_vector MP3(0);

  int lngFileCount = 0;

  if (strMusicSource == "")
    strMusicSource = Config.strdefault_music_source;

  if (FileExists(strMusicSource)) {
    // There is a file with this name. Is it an M3U?
    if (StringToLower(Right(strMusicSource, 4)) == ".m3u")
      blnUseM3U = true;
    else {
      log_error("File listed for Profile is not an M3U: " + strMusicSource);
      // Not an M3U file! Default to the standard music folder
      strMusicSource = Config.strdefault_music_source;
    }
  }
  else {
    // No file - is there a folder?
    if (!DirExists(strMusicSource)) {
      // No folder with this name! Default to the standard MP3 folder.
      // Report an error
      log_error("File or directory not found for current music profile: " + strMusicSource + " - changing to \"default\" music profile");
      // Change the var, we default back to the "default" music.
      strMusicSource = Config.strdefault_music_source;
      blnUseM3U=false; //
    }
  }

  // Now build up the list from either a pre-generated playlist, or a predefined folder
  if (blnUseM3U) {
    // Use a playlist file (M3U)
    int intLinesAdded=0;
    if (LoadM3UIntoPlaylistVector(strMusicSource, MP3, intLinesAdded)) {
      lngFileCount += intLinesAdded;
    }
    else {
      log_error("Error reading M3U file \"" + strMusicSource + "\" - changing to \"default\" music profile");
      strMusicSource = Config.strdefault_music_source;   // We default back to the "default" music.
      blnUseM3U = false;
    }
  }

  if (!blnUseM3U) { // Not using an else - the first part (check if M3U used) can fail, so we want to use default music in that case
    // Use a folder.
    strMusicSource = ensure_char_at_end(strMusicSource, '/');

    // Add all the MP3 files to the list
    // Change in player version 6.14 - allow symbolic links also.
    dir_list Dir_MP3(strMusicSource, ".mp3", DT_REG | DT_LNK);
    string strFileName = Dir_MP3.item();
    while (strFileName != "") {
      lngFileCount++;
      MP3.resize(lngFileCount);
      MP3[lngFileCount-1] = strMusicSource + strFileName;
      strFileName = Dir_MP3.item();
    }

    // Add all the M3U files to the list - these can be symbolic links also
    dir_list Dir_M3U(strMusicSource, ".m3u", DT_REG | DT_LNK);
    strFileName = Dir_M3U.item();
    while (strFileName != "") {
      // Change in player version 6.02 - open the m3u file, load lines into the vector, increase lngFileCount
      int intLinesAdded;
      if (LoadM3UIntoPlaylistVector(strMusicSource + strFileName, MP3, intLinesAdded)) {
        lngFileCount += intLinesAdded;
      }
      else {
        rr_throw("Error reading M3U file \"" + strMusicSource + strFileName + "\"");
      }
      strFileName = Dir_M3U.item();
    }
  }

  // Quit with an error if no MP3 or M3U files were found
  if (lngFileCount == 0) {
    rr_throw("No MP3 or M3U files found");
  }

  // Scramble the filename array
  srand(Now());
  // - Make 10 passes through the array
  string strFileName = "";
  for(int lngOuter=0; lngOuter<=9;lngOuter++) {
    for(int lngInner=0; lngInner<lngFileCount; lngInner++) {
      int lngRandomPos = rand() % lngFileCount;
      // Swap the two filenames
      strFileName = MP3[lngInner];
      MP3[lngInner] = MP3[lngRandomPos];
      MP3[lngRandomPos] = strFileName;
    }
  }

  // A new change - "disabled" mp3s - these are files to not be played. The user doesn't like them
  // for some reason. They are listed in tblplayeroutput also. Fetch them and remove any from the
  // playlist...
  string strSQL = "SELECT strmessage FROM tblplayeroutput WHERE strmsgdesc = " + psql_str("disabled");
  pg_result RS = DB.exec(strSQL);
  // Now load all the "disabled" mp3s into memory, use this list for a more efficient "playlist culling"
  // process
  string_hash_set DisabledMP3s;

  while (!RS.eof()) {
    vector <string> substrings;
    string_splitter split(RS.field("strmessage", ""), "||");  
    string strDisabledMP3=split;
    if (strDisabledMP3 != "") {
      DisabledMP3s.insert(strDisabledMP3); // Inserting the same key twice has no effect, don't check...
    }
    RS.movenext();
  }

  // We've loaded all the "disabled" mp3 paths. Now remove them from the playlist.
  string_vector::iterator MP3_item;
  MP3_item = MP3.begin();
  while(MP3_item != MP3.end()) {
    if (KeyInStringHashSet(DisabledMP3s, *MP3_item)) {
      MP3_item = MP3.erase(MP3_item);
    }
    else {
      ++MP3_item;
    }
  }

  // Check for and remove duplicate playlist entries:
  MP3_item = MP3.begin();
  while (MP3_item != MP3.end()) {
    // Check for entries after this point that have the same filename:
    string strFile=*MP3_item;
    string_vector::iterator MP3_item_check;
    MP3_item_check=MP3_item;
    ++MP3_item_check;

    while (MP3_item_check != MP3.end()) {
      if (*MP3_item_check == strFile) {
        log_warning("Removing duplicate playlist entry: " + strFile);
        MP3_item_check = MP3.erase(MP3_item_check);        
      }
      else {
        ++MP3_item_check;
      }
    }
    ++MP3_item;    
  }
  
  // Add the filenames to the playlist
  // - First build a playlist file
  ofstream playlist((GetExecDir() + "playlist.m3u").c_str());
  if (playlist) {
    for(int intIndex = 0; intIndex < (int)MP3.size(); intIndex++)
      playlist <<  MP3[intIndex] << endl;
    playlist.close();
    // Now that we have the new playlist, feed it to the MediaPlayer object
    // MediaObject will decide whether to interrupt the currently playing song.
    // Also MediaObject will log an error if there are too few MP3s playing.

    // Important: Full path must be passed because XMMS has a different
    // "current working directory" to the player.
    MediaPlayer.SetMusicPlaylist(GetExecDir() + "/playlist.m3u");

    // CreateRandomPlaylist should do just that - it's return value determines if the playlist changed, ie it should
    // actually be played by the MediaPlayer object.
  }
  
  // Set a variable which reminds the main program that the playlist and available music need to be logged.
  // - This is so that this function can return quickly, and these (sometimes time consuming) operations can be handled
  // a bit later.
  blnMusicLoggingNeeded = true;
}

// Sub-function for CreateRandomPlayList
bool player::LoadM3UIntoPlaylistVector(const string &strm3upath, vector<string> &music_list, int &intLinesAdded) {
  // open an m3u file, read all it's lines into the music_list vector. Ignore lines starting with "#"
  // Return how many lines were added to music_list, in lngLinesAdded
  try {
    intLinesAdded = 0;  // No lines added yet

    ifstream inFile(strm3upath.c_str());

    if (!inFile) {
      // Unable to open file - return False
      return false;
    }
    else {
      // Read in every line and process it
      char ch_FileLine[2048] = "";
      string strLine;
      while (inFile.getline(ch_FileLine, 2047)) {
        // We now have a file line. Process it.
        strLine = ch_FileLine;
        strLine = trim(strLine); // Remove leading and trailing spaces

        // Ignore the line if it starts with # or if it is empty.
        if (!(strLine.length() == 0 || strLine[0] == '#')) {
          // Add the line to music_list
          music_list.resize(music_list.size() + 1);
          music_list[music_list.size() - 1] = strLine;
          ++intLinesAdded;
        }
      }
    }

    // Successfully extracted all the lines.
    // Return a success value to the calling function
    return true;
  } catch_exceptions;
  return false;
}

/*******************************
    MediaPlayer stuff
*******************************/

void player::MediaPlayer_Play() {
  // Call this instead of MediaPlayer.Play, because it also does some database logging for
  // external processes to check..
  MediaPlayer.Play();
  // After the music playback has been started, check the music type and log it.
  WriteLiveInfoSetting("Music source", ((MediaPlayer.GetMusicType()==xmms) ? "xmms":"linein"));
}

/***********************************
    Handle Announcements
************************************/
void player::CorrectWaitingAnnouncements() {
  // Check the DB for announcements that are marked as 'waiting to play' ie, enqueued. If the player
  // Is not currently playing back announcements then there should not be any announcements
  // like this. Correct any, and log that there was a correction
  try {
    string strsql = "SELECT lngtz_slot FROM tblschedule_tz_slot WHERE bitscheduled = '" + itostr(AdvertListedToPlay) + "'";
    pg_result RS = DB.exec(strsql);
    long lngcorrected = RS.recordcount();

    // Now run a query to fix all these hanging 'waiting' announcements.
    strsql = "UPDATE tblschedule_tz_slot SET bitscheduled = '" + itostr(AdvertSNSLoaded) + "' WHERE bitscheduled = '" + itostr(AdvertListedToPlay) + "'";
    DB.exec(strsql);

    // Log how many were corrected
    if (lngcorrected > 0) {
      log_message(itostr(lngcorrected) + " announcement(s) corrected. (Changed status from 'about to be played' to 'loaded from SNS').");
    }
  } catch_exceptions;
}

bool player::doHandleMediaPlayerAnnPlayback(ann_playback_status & Status) {
  // After queueing the announcements in the MediaPlayer object, and starting
  // the announcement playback with StartAnnouncementQueuePlay(), this
  // function is called to kick off the playback, monitor the playback, and take
  // any necessary functions. True is returned if the function was successful,
  // False is returned if the announcement playback had a problem
  try {
    static long lngLastAnnID = -1; // Used for knowing when new announcements are playing.    

    // Wait for 1/10th of a second, and then tell AnnPlaybackCheck that 100 ms have elapsed
    // since the last time the volumes queue was checked.
    usleep(1000000/10);
    bool blnUserStopped = !PlaybackEnabled(); // Check if the user stopped playback..
    // In the while loop condition below, the positioning of "blnPlaybackEnabled" is important!
    while (!blnUserStopped && MediaPlayer.AnnPlaybackCheck(Status)) {
      // Here we check the Status variable
      // - Don't bother checking the blnComplete field, we know it is false.
      // Check if a new announcement is playing... (-1 means uninitialized...)
      if (Status.lngAnnounceID != -1 && Status.lngAnnounceID != lngLastAnnID) {
        // Detected a new announcement starting to play. Mark the previous one as complete
        if (lngLastAnnID != -1) {
          MarkAnnounceComplete(lngLastAnnID);
        }
        lngLastAnnID = Status.lngAnnounceID; // Wait for the announcement to change again...
      }

      // Added in 6.17 - If XMMS stops, check for a "STOP" command in the database..
      if (Status.blnStopped) {
        Process_WaitingCMDs();              // If the user chose to stop XMMS from the wizard, there will be a STOP
                                                             // command queued in tblwaitingcmd...
        // Check if the playback loop should continue..
        blnUserStopped = !PlaybackEnabled();
      }

      // Sleep a bit, ie wait a bit between calls to AnnPlaybackCheck
      usleep(1000000/10); // Call AnnPlaybackCheck every 1/10th of a second.
    }

    // If execution reaches this point, either the announcements finished playing,
    // or the announcement playback was cancelled (eg: the user clicked on a "stop" button which
    // created a player stop output command). Alternately there could be an unexpected
    // error in announcement playback which could not be resolved.

    // Was the announcement playback cancelled?

    // Removed in version 6.17:
//    bool blnCancel = Status.blnUserStopped || Status.blnUserPaused || Status.blnUnexpectedErr;
    bool blnCancel = Status.blnUserPaused || Status.blnUnexpectedErr || blnUserStopped;

    if (blnCancel) {
      // If playback was cancelled (probably by the user clicking a Stop or Pause button) then
      // check for new player commands - probably there is now an instruction to
      // STOP or PAUSE the player.
      if (blnUserStopped) {
        log_message("Detected: User stopped XMMS");
      }
      else if (Status.blnUserPaused) {
        log_message("Detected: User paused XMMS");
      }
      else if (Status.blnUnexpectedErr) {
        log_message("Detected: Unexpected XMMS state.");
      }

      // Now reset the MediaPlayer announcement queue if there are incomplete
      // announcements. Next time we add announcements,
      // the announcement queue should be empty.

      // The Announcement Queue only contains ads that haven't started playing - the currently playing
      // ad (which was just interrupted) is not included. So Get the 'waiting' announcement queue, and add 1
      int intIncompleteQueueAds = MediaPlayer.GetAnnouncementQueueLength() + 1;
      if (intIncompleteQueueAds != 0) {
        // There are previous announcements queued!
        log_warning("Because of this condition, " + itostr(intIncompleteQueueAds) + " announcement(s) failed. They will be retried later.");;
        MediaPlayer.ResetAnnouncements();
      }
  
      // Check for any commands that could have been inserted into the schedule database
      // in conjunction with the xmms pause command. Possibly an external user really does
      // want to pause XMMS.
      Process_WaitingCMDs();
    }
    else {
      // Playback was not cancelled, ie it finished, so log that the music will resume
      // - Mark the last announcement that played, as complete:
      if (lngLastAnnID != -1) {
        MarkAnnounceComplete(lngLastAnnID);
      }
      return true; //  Announcement playback finished without errors.
    }
  } catch_exceptions;
  return false; // If execution reaches this point, there was an error.
}

void player::WriteErrorsForMissedAds() {
  // Look for ads from today that are older than [Config.intMinsToMissAdsAfter] minutes
  // have not yet been played (or scheduled to play)
  try {
    string psql_EarliestTime = TimeToPSQL(Time()- (60 * Config.intMinsToMissAdsAfter));

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
       " AND (tblSchedule_TZ_Slot.bitScheduled = " + itostr(AdvertSNSLoaded) + ") "
       " AND (COALESCE (tblschedule_tz_slot.bitMissErrorWritten, '0') = '0')"

       // Ordering
       " ORDER BY tblSched.strFileName, tblSchedule_TZ_Slot.lngTZ_Slot";

    pg_result RS = DB.exec(strSQL);

    // We need to gather statistics for each missed announcement (first time missed, last time, number)
    // and display summaries of each. The query is sorted by filename to facilitate this also.
    string strmissed_file = "";
    string strmissed_prev_file="";
    long lngmissed_count=0;
    DateTime dtmmissed_first=datetime_error;
    DateTime dtmmissed_last=datetime_error;

    while (!RS.eof()) {
      string strTZ_Slot = RS.field("lngTZ_Slot");
      DateTime dtmDay  = parse_psql_date(RS.field("dtmDay"));
      DateTime dtmTime = parse_psql_time(RS.field("dtmForcePlayAt", RS.field("dtmStart", " ").c_str()));
      DateTime dtmPlayAt = dtmDay + dtmTime - timezone;

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
          WriteErrorsForMissedAds_logmissed(strmissed_prev_file, lngmissed_count, dtmmissed_first, dtmmissed_last);
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
      DB.exec(strSQL);

      // Now move to the next record.
      RS.movenext();
    }

    // Now, at the end of the loop, write the details for the last missed announcement that we
    // were processing above.
    if (strmissed_prev_file != "") {
      // Yes, so print out the stats gathered for the last processed filename.
      WriteErrorsForMissedAds_logmissed(strmissed_prev_file, lngmissed_count, dtmmissed_first, dtmmissed_last);
    }
  } catch_exceptions;
}

void player::WriteErrorsForMissedAds_logmissed(const string strmissed_file, const long lngmissed_count, const DateTime dtmmissed_first, const DateTime dtmmissed_last) {
  // Log an error for "announcement missed" stats that have been gathered. This function is used
  // only by the "WriteErrorsForMissedAds() function.
  try {
    string strmissed_first = format_datetime(dtmmissed_first, "%DD/%MM/%YYYY %HH:%NN:%SS");
    string strmissed_last = format_datetime(dtmmissed_last, "%DD/%MM/%YYYY %HH:%NN:%SS");

    // v6.15 - Missed adverts are logged as messages instead of errors, ie, logged but not e-mailed.
    if (lngmissed_count == 1) {
      log_warning("Announcement " + strmissed_file + " (at " + strmissed_first + ") was missed.");
    }
    else {
      log_warning("Announcement " + strmissed_file + " was missed " + itostr(lngmissed_count) + " times between " + strmissed_first + " and " + strmissed_last);
    }
  }
  catch_exceptions;
}

void player::MarkAnnounceComplete(const long lngTZ_Slot) {
  // This is an internal function used only by doHandleMediaPlayerAnnPlayback. It is called when
  // an announcement plays, to log to the database that it has played.
  try {
    // ==============================================================================
    // Update tblSchedule_TZ_Slot
    string strSQL = "UPDATE tblSchedule_TZ_Slot SET bitScheduled = " + itostr(AdvertPlayed) +
                             ", dtmPlayedAtDate = " + psql_date + ", dtmPlayedAtTime = " + psql_time +
                             ", bitplayed = '1' "
                             " WHERE (lngTZ_Slot=" + itostr(lngTZ_Slot) + ")";

    DB.exec(strSQL);
  } catch_exceptions;
}

/***********************
    Volume zones                  
***********************/
void player::volZones() {
  try {
    // Firstly, check if the store is closed (ie, it is after store hours)
    if (SemiSonic() == "CLOSE") {
      // Store is now closed. All volumes go to 0
      if (CurrentStatus.curVolume != 0) {
        CurrentStatus.curMusicVolume = 0;
        CurrentStatus.curDefaultAnnounceVol = 0;
        CurrentStatus.curVolume = 0;
        MediaPlayer.SetVolumeLevels(0, 0, 0);
      }
    }
    else {
      // Store is now open. Check the database for the correct volume settings
      // - Build a query to fetch the current volume zone (ie: Hour) details
      string strSQL = "SELECT * FROM tblVolumeZones WHERE intDayNumber = " +
         itostr(Weekday(Now())) + " AND lngTimeZone = " +
         itostr((GetDateTimeTime(Now())-timezone)/(60*60) + 1);

      pg_result RS = DB.exec(strSQL);

      CurrentStatus.curAdjVol = RS.eof()?0:strtoi(RS.field("intVolAdj", "0"));

      // Now run a query to fetch main volume details from table tblstore
      RS = DB.exec("SELECT * FROM tblStore");
      if (!RS.eof()) {
        CurrentStatus.curMusicVolume = strtoi(RS.field("intMusicVolume", "45")) + CurrentStatus.curAdjVol;
        CurrentStatus.curDefaultAnnounceVol = strtoi(RS.field("intAnnVolume", "90")) + CurrentStatus.curMusicVolume;
      }

      // A new setting - the external (linein) volume. Default is meant to be 100%
      CurrentStatus.curLineInVol = strtoi(LoadSettingFromDB("intLineInVol", "255", "int"));

      // Now we update the Media Player volume levels
      MediaPlayer.SetVolumeLevels((CurrentStatus.curMusicVolume*100)/255, (CurrentStatus.curDefaultAnnounceVol*100)/255, (CurrentStatus.curLineInVol*100)/255);
    }
  } catch_exceptions;
}

/***********************
    Music History
***********************/

void player::LogLatestMusicMP3Played() {
  // Check what music is currently being played, compare it with the last played music.
  // If it is a new song then add it to the music history list (DB and listview)
  try {
    if (!MediaPlayer.IsPlaying()) return; // Quit if there is no media playing now.

    // Retrieve the current song title. Sometimes the title cannot be determined (eg: external music
    // feed)
    string strDescr = MediaPlayer.GetSongTitle();

    // Quit if required information could not be retrieved
    if (strDescr == "") return;

    // Fetch the most-recently-added song description from the database
    string strPrevSong = "";
    string strSQL = "SELECT strdescription FROM tblMusicHistory ORDER BY lngplayedmp3 DESC LIMIT 1";
    pg_result RS = DB.exec(strSQL);

    if (!RS.eof())
      strPrevSong = RS.field("strDescription", "");

    // Quit if this song is still playing
    if (strPrevSong==strDescr) return;

    // If we are at this point, then a new song is playing and it must be added to the history.
    log_message("Playing music: " + strDescr);

    // Also, some new functionality: When the player detects a new MP3, log the current XMMS status
    LogXMMSStatusToDB();

    strSQL = "INSERT INTO tblMusicHistory (dtmTime, strDescription) VALUES (" + psql_now + ", " + psql_str(strDescr) + ")";
    RS = DB.exec(strSQL);

    // If there are more than 100 MP3s listed then erase the oldest MP3s
    strSQL = "SELECT COUNT(lngPlayedMP3) AS Counter FROM tblMusicHistory";
    RS = DB.exec(strSQL);

    int intCounter = 0;

    if (!RS.eof())
      intCounter = strtoi(RS.field("Counter", "0"));

    if (intCounter > 100) {
      // There are more then 100 MP3's Listed - erase the oldest
      strSQL = "SELECT lngPlayedMP3 FROM tblMusicHistory ORDER BY lngPlayedMP3 LIMIT " + itostr(intCounter - 100);
      RS = DB.exec(strSQL);
      while (!RS.eof()) {
        strSQL = "DELETE FROM tblMusicHistory WHERE lngPlayedMP3 = " + RS.field("lngPlayedMP3", "-1");
        DB.exec(strSQL);
        RS.movenext();
      }
    }
  } catch_exceptions;
}

/*****************************************
    Received directory handling
*****************************************/

void player::Check_Received() {
  // this procedure checks the apppaths.recieved directory for the following files to process
  // .cmd
  string FileName; // used to read file names from a folder.
  string Full_Path;

  try {
    // Go through all files in the received folder and where necessary reset their
    // Read-Only attribute
    ClearReadOnlyInDir(Config.AppPaths.strreceived);

    // Process command files
    dir_list Dir_CMD(Config.AppPaths.strreceived, ".cmd");
    FileName = Dir_CMD.item();
    while (FileName != "") {
      // Create the full path
      Full_Path = Config.AppPaths.strreceived + FileName;
      log_message("Processing CMD file: ");
      // Load the command file into the database
      if (Load_CMDIntoDB(Full_Path)) {
        remove(Full_Path.c_str());
        Process_WaitingCMDs();
      }
      else {
        log_error("An error occured trying to load the following file: " + Full_Path);
        remove(Full_Path.c_str());
      }
      // Next file
      FileName = Dir_CMD.item();
    }
  } catch_exceptions;
}

/****************
    LiveInfo
****************/

void player::WriteLiveInfoSetting(const string strName, const string strValue) {
  // There is a tblLiveInfo table in the DB that stores the same information as
  // the liveinfo.chk file. This procedure is used to update one of these settings
  try {
    string strSQL;

    strSQL = "SELECT lngStatus FROM tblliveInfo WHERE strstatusname = " + psql_str(strName);
    pg_result RS = DB.exec(strSQL);
    // Generate part of the SQL string - if strValue is empty then a NULL value
    // must be written.
    if (RS.eof())
      // A new status setting - INSERT
      strSQL = "INSERT INTO tblliveinfo (strstatusname, strstatusvalue) VALUES (" + psql_str(strName) + ", " + psql_str(strValue) + ")";
    else
      // An existing one - UPDATE
      strSQL = "UPDATE tblliveinfo SET strstatusvalue = " + psql_str(strValue) + " WHERE strstatusname = " + psql_str(strName);

    DB.exec(strSQL);
  } catch_exceptions;
}

void player::doUpdateLiveInfo() {
  // Player version 6.15 - removing use of loaderinfo.tmp and liveinfo.chk.
  // liveinfo is a collection of information about the store computer's running software
  try {
    // Kill liveinfo.chk if it exists
    string LiveInfoChk = Config.AppPaths.strtoday + "liveinfo.chk";
    remove(LiveInfoChk.c_str());

    // Get the store IP address, store code, store name, music volume, announcement volume
    string strSQL = "SELECT * FROM tblStore";
    pg_result RS = DB.exec(strSQL);

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
    RS = DB.exec(strSQL);

    string strNumAdsToday;
    if (!RS.eof()) {
      strNumAdsToday = RS.field("Counter");
    }
    else {
      strNumAdsToday = "0";
    }

    // Get the description for the currently-playing profile
    strSQL = "SELECT strprofilename FROM tblmusicprofiles WHERE strmusic = " + psql_str(strMusicSource);
    RS =  DB.exec(strSQL);

    string strProfileName = "";

    if (!RS.eof())
      strProfileName = RS.field("strprofilename", "<Profile name error>");
    else
      strProfileName = "Default profile";

    // Get string forms of the date and time
    string strDate = format_datetime(Date(), "%dd/%mm/%yyyy");
    string strTime = format_datetime(Time(), "%hh:%nn:%ss %AM/PM");

    // Also update the tblLiveInfo table
    WriteLiveInfoSetting("Date", strDate);
    WriteLiveInfoSetting("Time", strTime);
    WriteLiveInfoSetting("Music vol", strMusicVol);
    WriteLiveInfoSetting("Announce vol", strAnnouncementVol);
    WriteLiveInfoSetting("Adjustment vol", itostr(lrint(CurrentStatus.curAdjVol)));

    WriteLiveInfoSetting("Ads today", strNumAdsToday);
    WriteLiveInfoSetting("Player version", strPlayerVer);
    WriteLiveInfoSetting("Music profile", strProfileName);
  } catch_exceptions;
}

/************************************************
    Store's OPEN or CLOSED status
************************************************/

string player::SemiSonic() {
  // This func returns a "OPEN" if the store should be open or a "CLOSE" if the store should be closed
  string strRet = "OPEN";
  try {
    string strSQL =
      "SELECT dtmOpeningTime, dtmClosingTime FROM tblStoreHours WHERE intDayNumber = " +
      itostr(Weekday(Now()));

    DateTime dtmOpeningTime = datetime_error;
    DateTime dtmClosingTime = datetime_error;

    pg_result RS = DB.exec(strSQL);
    if (!RS.eof()) {
      dtmOpeningTime = parse_psql_time(RS.field("dtmOpeningTime"));
      dtmClosingTime = parse_psql_time(RS.field("dtmClosingTime"));
    }

    // Some error-trapping code - if the fields were null then use default values
    if (dtmOpeningTime == datetime_error) {
      dtmOpeningTime = MakeDateTime(0,0,0,7,0,0);
      log_error("Could not read Store Opening time from the Schedule database");
    }
    if (dtmClosingTime == datetime_error) {
      dtmOpeningTime = MakeDateTime(0,0,0,19,0,0);
      log_error("Could not read Store Closing time from the Schedule database");
    }

    // Now make sure that the Opening and Closing times have no "date" components:
    dtmOpeningTime = GetDateTimeTime(dtmOpeningTime);
    dtmClosingTime = GetDateTimeTime(dtmClosingTime);    

    // Now decide on what value to return. Also set a global variable
    if (Time() >= dtmOpeningTime && Time() < dtmClosingTime)
      strRet = "OPEN";

    if (Time() >= dtmClosingTime || Time() < dtmOpeningTime) {
      strRet = "CLOSE";
    }

    return strRet;
  } catch_exceptions;

  return "";
}

void player::StoreClose() {
  // This simulates closing the store
  // The necessary functions and timers are stopped
  try {
    log_message("Store closing for the evening - playback stopped");
    // Update status vars
    CurrentStatus.StoreStatus = "CLOSE";

    // Stop playback, try our best to stop any chance of music playback.
    MediaPlayer.Stop();
    MediaPlayer.SetVolumeLevels(0, 0, 0);
  } catch_exceptions;
}

void player::StoreOpen() {
  // Opens the store (this procedure will load all the necessary values
  // and starts the applicable procedures to simulate an open store.
  try {
    log_message("Store opening for the day (RR Date: " + DateTimeToRRDate(Date()) + ") - resuming player functions...");
    Check_Received();

    CurrentStatus.StoreStatus = "OPEN";

    volZones(); // Setup volumes
    CheckMusicProfile(true); // Check the current music profile, always rebuild the playlist.
    MediaPlayer_Play(); // Start playback of the playlist that has been set.

    // Also cancel any paused or stopped state
    CurrentStatus.blnMediaPaused = false;
    CurrentStatus.blnMediaStopped = false;
  } catch_exceptions;
}

/***************************************************************************
    Log XMMS playlist and available music to the database
***************************************************************************/

void player::LogAllMachineMusicToDB() {
  // Use the current music profile path to log the available mp3s to the database
  try {
    log_message("Logging all available music to database... (please be patient)");

    const string strAvailMP3sDescr = "avail_mus";
    const string strDisabledMP3sDescr = "disabled";

    // Remove all avail_mus records
    string strSQL = "DELETE FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strAvailMP3sDescr);
    DB.exec(strSQL);

    // Make an in-memory list of all the disabled MP3s
    strSQL = "SELECT strmessage FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strDisabledMP3sDescr);
    pg_result RS = DB.exec(strSQL);

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

    // Build up a linux command to list all of the machine's music MP3s into a text-file
    // - The textfule is "avail_music.txt"
    string strCommand = "ls " + Config.AppPaths.strmp3 +  "*.[Mm][Pp]3 > " + GetExecDir() + "avail_music.txt; "
                        "find " + Config.AppPaths.strprofiles + " | grep \"\\.[Mm][Pp]3\" >> " + GetExecDir() + "avail_music.txt";
    system(strCommand.c_str());

    // Open playlist.m3u and read all the lines. Extract the mp3 filename out of the paths
    ifstream AvailMusicFile((GetExecDir() + "avail_music.txt").c_str());
    if (AvailMusicFile) {
      char ch_FileLine[2048] = "";

      // Create a database transaction to speed up the inserts:
      pg_transaction T(DB);
      
      while (AvailMusicFile.getline(ch_FileLine, 2047)) {
        string strLine = trim(ch_FileLine);

        // Now skip listing this file under "available music" if it is listed under "disabled music"
        if (!KeyInStringHashSet(DisabledMP3s, strLine)) {
          // Now that we have the line, attempt to get the MP3 title
          string strTitle = MediaPlayer.GetMP3Title(strLine);

          // Decide on the final line, and also prepend the path and ||
          if (strTitle != "")
            strLine = strLine + "||" + strTitle;
          else
            strLine = strLine + "||" + GetShortFileName(strLine);

          if (strLine != "") {
            strSQL = "INSERT INTO tblplayeroutput (strmessage, strmsgdesc, dtmtime) VALUES (" + psql_str(strLine) + ", " + psql_str(strAvailMP3sDescr) + ", " + psql_time + ")";
            T.exec(strSQL);
          }
        }
      }

      // Now commit the database transaction:
      T.commit();      
      
      AvailMusicFile.close();
      // Erase the file now
      remove("avail_music.txt");
    } else log_error("avail_music.txt not found");
  } catch_exceptions;
}

void player::LogXMMSPlaylistToDB() {
  // Generate a playlist.txt to list the current playlist into the tblplayeroutput table
  try {
    log_message("Logging XMMS playlist to database...");

    const string strPlaylistDescr = "playlist";

    // Remove all playlist records
    string strSQL = "DELETE FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strPlaylistDescr);
    DB.exec(strSQL);

    // If the playlist file exists then read it into the tblplayeroutput table
    ifstream PlaylistFile((GetExecDir() + "playlist.m3u").c_str());
    if (PlaylistFile) {
      char ch_FileLine[2048] = "";
      while (PlaylistFile.getline(ch_FileLine, 2047)) {
        string strLine = trim(ch_FileLine);

        // Now that we have the line, attempt to get the MP3 title
        string strTitle = MediaPlayer.GetMP3Title(strLine);

        // Here we also prepend the path and "||"
        if (strTitle != "")
          strLine = strLine + "||" + strTitle;
        else
          strLine = strLine + "||" + GetShortFileName(strLine);

        if (strLine != "") {
          strSQL = "INSERT INTO tblplayeroutput (strmessage, strmsgdesc, dtmtime) VALUES (" + psql_str(strLine) + ", " + psql_str(strPlaylistDescr) + ", " + psql_time + ")";
          DB.exec(strSQL);
        }
      }
    }
    else log_error("file not found: playlist.m3u");
    PlaylistFile.close();
  } catch_exceptions;
}

/***************************************************
    Log XMMS's status to the database
***************************************************/

void player::LogXMMSStatusToDB() {
  // into the tblplayeroutput table
  try {
    const string strXMMS_Status = "mp_status";

    // Delete all of the mp_Status records
    string strSQL = "DELETE FROM tblplayeroutput WHERE strmsgdesc = " + psql_str(strXMMS_Status);
    DB.exec(strSQL);
    
    // Now use XMMS api calls to generate various status lines
    // - Output in a format similar to the good old XMMS-SHELL (with some revisions)
    Write_tblPlayerOutput("Playing: " + GetShortFileName(MediaPlayer.GetSongPath()) + " - " + MediaPlayer.GetSongTitle(), strXMMS_Status);

    // Calculate the time
    Write_tblPlayerOutput("Time: " + MediaPlayer.GetSongTimeStr() + (MediaPlayer.IsPlaying() ? string("") : (MediaPlayer.IsPaused()? string(" (paused)") : string(" (stopped)"))), strXMMS_Status);

    // Write lines for the left and right volumes (also convert back to % levels, not the internally used 0-255 levels)
    double dblVol = MediaPlayer.GetVolume();

    Write_tblPlayerOutput("Left volume: " + itostr(lrint(dblVol)), strXMMS_Status);
    Write_tblPlayerOutput("Right volume: " + itostr(lrint(dblVol)), strXMMS_Status);
  } catch_exceptions;
}

void player::Write_tblPlayerOutput(const string strMessage, const string strMessageDescr) {
  // Write an entry to tblplayeroutput. This function is used internally by the procs.cpp code
  try {
    // Only write details if strMessage and strMessageDescr are not empty
    if (strMessage != "" && strMessageDescr != "") {
      // Build the query
      string strSQL = "INSERT INTO tblplayeroutput (strmessage, strmsgdesc, dtmtime) VALUES (" + psql_str(strMessage) + ", " + psql_str(strMessageDescr) + ", " + psql_time + ")";
      // Run the query
      DB.exec(strSQL);
    } else log_error("Senseless code alert - An empty parameter was passed to this function");
  } catch_exceptions;
}

/**********************
  RAT code stuff
**********************/

// RAT is a system used for checking command files

player::enumRAT player::RAT(const string strInputLine) {
  // radio retail formula, version RAT
  try{
    string DeathOfRats = substr(strInputLine, 0, 4);

    char Mort = substr(strInputLine, 4, 1).c_str()[0];
    string Blinky = substr(strInputLine, 5, 4);

    enumRAT retVal = ratProblem;

    string StarFoHtaed, Wizzard;

    switch(Mort) {
      case 'R' : // Reverse
                 StarFoHtaed = substr(Blinky, 3, 1) + substr(Blinky, 2, 1) + substr(Blinky, 1, 1) + substr(Blinky, 0, 1);
                 if ((9090 - strtoi(DeathOfRats)) == strtoi(StarFoHtaed))
                   retVal = ratUnEncrypted;

                 break;
      case 'A' : // [a]lternate #0
                 Wizzard = 9090 - strtoi(DeathOfRats);
                 if ((substr(Wizzard, 0, 1) == substr(Blinky, 0, 1)) &&
                     (substr(Blinky, 1, 1)=="0") &&
                     (substr(Wizzard, 2, 1) == substr(Blinky, 2, 1)) &&
                     (substr(Blinky, 3, 1)=="0"))
                   retVal = ratUnEncrypted;
                 break;
      case 'T' : // [t]rue
                if (9090 - strtoi(DeathOfRats) == strtoi(Blinky))
                  retVal = ratUnEncrypted;
                break;
    }
    return retVal;
  } catch_exceptions;
  return ratProblem; // return was meant to happen earlier!
}

// *************************************************************************************************************
//   Support for times when the next announcement batch should be started immediatly:
// *************************************************************************************************************

void player::do_next_announce_batch_immediately(bool blnimmed) { // Set the state
  blndo_next_announce_batch_immediately = blnimmed;

  // Also, disable music playback if:
  // 1) The next announcement batch is to be triggered immediately, or
  // 2) Player Playback is disabled (store hours, etc)
  // - This will stop the MediaPlayer maintenance and various other checks from starting up music
  // when we're in a "back-to-back" advert playback state.
  MediaPlayer.MusicEnabled(!blnimmed && PlaybackEnabled());
}

bool player::do_next_announce_batch_immediately() { // Get the state
  return blndo_next_announce_batch_immediately;
}
