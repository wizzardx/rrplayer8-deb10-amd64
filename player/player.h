/***************************************************************************
                          player.h  -  description
                             -------------------
    begin                : Sun Jun 16 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifndef PLAYER_H
#define PLAYER_H

// Finally some sort of attempt to tidy up the player's code structure

#include <string>
#include "media_player.h"
#include "player_logger.h"
#include "rr_psql.h" // For Connection
#include "timed_events.h" // For running events at specified intervals.

// Path for the player to run in
const string player_path = "/data/radio_retail/progs/player/";
// Filename for the playing.chk file..
const string playing_chk = "playing.chk";

// Player's version, database version that it expects to run with.
const string strPlayerVer    = VERSION;   // The all-important player version constant
const string strCorrectDBVer = "3.19";    // And the schedule database (postgres) version
                                          // it's meant to run with. Higher DB versions usually won't hurt but
                                          // earlier DB versions can cause major problems.

// Default database connection settings
const string strDefaultMainDBServer	="127.0.0.1";
const string strDefaultMainDBName	="schedule";
const string strDefaultUser					="postgres";
const string strDefaultPassword			="postgres";
const string strDefaultPort					="5432";
                                                                 
// Some default player settings to use if they're not found in the database
const int intDefault_MinsToMissAdsAfter = 15; // 5 minutes
const int intDefault_MaxAdsPerBatch = 3; // 3 ads max per batch
const int intDefault_MinMinsBetweenAdBatches = 4; // At least 4 minutes of music between announcement batches.

// Length of music fade-out and fade-in during announcement playback:
const int intDefault_MusicFadeOutLength = 5000; // 5000ms = 5 seconds
const int intDefault_MusicFadeInLength = 5000; // 5000ms = 5 seconds

// Added in version 6.15 (build 211) - a default setting for XMMS latency if it is not found
// in the database.
const int intDefault_ARTSLatency = 1000; // Assume an aRts latency of 1 second.

// Advert status (used by tblSchedule_TZ_Slot.bitScheduled)
enum advert_status_type {
  AdvertSNSLoaded    = 0,
  AdvertListedToPlay = 1,
  AdvertPaused       = 2,
  AdvertDeleted      = 3,
  AdvertPlayed       = 4 
};

// Version 6.15 (build 332) - The player now checks for alterations to tblapppaths.strmp3 and corrects them to the
// the value below (see the logic in player::set_Paths()) :
const string strcorrect_mp3_path = "/data/radio_retail/stores_software/data/";

// Forward declarations to classes that we will define as being "friends" of the
// player class.
class event_check_music;
class event_check_received;
class event_check_waiting_cmds;
class event_operational_check;
class event_scheduler;
class event_check_db;
class event_player_running;
class event_check_music_db_err;

class player {
public:
  // Main methods
  player();    // Initialize the player
  ~player();  // Close down the player
  bool do_events(); // Check for any player events that need to run, run them, and return.
                               // returns false if it is time for the player to quit.

  // Operators

  // - bool operator - lets you retrieve a boolean value from the object variable itself, not a method or attribute.
  // - The value is true if the Player initialization succeeded.
  operator bool() { return blninit_success; }

private:
  // Information set by the constructor...
  // Did the player start successfully?
  bool blninit_success;

  bool blnTerminatePlayer; // Set this to true to end the player event loop

  pg_connection DB; // Database connection to the schedule database

  // The timed events that take place within the Player
  event * timCheckMusic;
  event * timReceivedCheck;
  event * timCheckWaitingCMDs;
  event * timOperationalCheck;
  event * timScheduler;
  event * timCheckDBConn;
  event * timShowPlayerRunning;
  event * timCheckMusicDbErr; // Called by the database handling when there are retries...

  // Friend classes of this class - they are in fact functionality of this class that have been split off
  // because they are run by this class at specific intervals.
  friend class event_check_music;
  friend class event_check_received;
  friend class event_check_waiting_cmds;
  friend class event_operational_check;
  friend class event_scheduler;
  friend class event_check_db;
  friend class event_player_running;
  friend class event_check_music_db_err;

  // An object that is called regularly to run all the events at the correct intervals:
  timed_events EventRunner;
  
//  // Log important messages to the database.
//  void LogError(const string strType, const string strMsg, const string strFrom);
//  void LogMessage(const string strType, const string strMsg, const string strFrom);

//  // Handle unexpected function errors
//  void ErrorHandler(const string ProcName, const string Descr = "Program error", const string Priority = "MEDIUM");
  
  // Load and save settings from the database
  string LoadSettingFromDB(const string strSettingName, const string strDefault, const string strType);
  void SaveSettingToDB(const string strSettingName, const string strType, const string strValue);

  // Load paths from the database
  void set_Paths();

  // Load other (non-path) database settings.
  void Load_ConfigFromDB(); // Load player config options from the database...
  
  // Config file
  void ReadPlayerConfigFile();

  // Some settings and configuration read from the config file and from the database.
  struct {
    // Database connection details (player.conf)
    struct {
      string strDBServer;
      string strDBName;  // The name of the MySQL database
      string strUserName;  // The user name
      string strPassword;  // The password
      string strPort;  // The port
    } MainDBParams;

    // Announcement frequency capping options (tbldefs)
    int intMinsToMissAdsAfter;   // If an announcement was scheduled to play earlier than this amount of time ago, then skip it if it has not already been played.
    int intMaxAdsPerBatch;
    int intMinMinsBetweenAdBatches; // Minimum amount of music to play between announcement batches

    // Application paths. (tblapppaths)
    struct {
      string strmain;
      string stradmin;
      string strmp3;
      string stradverts;
      string strannouncements;
      string strspecials;
      string strconfirmbroadcast;
      string stremergency;
      string strerror;
      string strplaylist;
      string strreceived;
      string strreturns;
      string strschedules;
      string strtemp;
      string strtoday;
      string strroot;
      string strinstoredb;
      string stragent;
      string strupdater;
      string strloader;
      string strplayer;
      string strprofiles;
    } AppPaths;

    // Added in 6.15 (build 330) The location to use for the default music profile:
    string strdefault_music_source;

    // Added in 6.21 (build 737) Wait for the current song to end before starting
    // announcement playback? Exception: linein music & "force to play now" adverts.
    bool blnAdvertsWaitForSongEnd;
  } Config;

  // Create the instore paths
  void Create_Directory_Structure();
  
  // Enable and disable the playback (ie: music and adverts)
  bool PlaybackEnabled(); // Return the current "enabled" status of music & ad playback.
//  void Playback_enable();
//  void Playback_disable();

  // Pause, Stop and Resume media playback
  void Media_Pause();
  void Media_Stop();
  void Media_Resume();  

  // Functions for handling player commands (stored in the database)
  bool Load_CMDIntoDB(const string Full_Path);
  void Process_WaitingCMDs();
  void Remove_WaitingMediaPlayer_CMDs(); 
  
  // Music profiles
  bool CheckMusicProfile(const bool blnForcePlaylistRebuild=false);
  string strMusicSource, strPrevMusicSource; // Variables used by CheckMusicProfile
  // A function used by internally by CheckMusicProfile:
  bool check_short_weekday(const string & strShortWeekDay, int & intWeekDay);  

  // Music Playlist
  void CreateRandomPlaylist();
  // And a function used only by CreateRandomPlayList:
  bool LoadM3UIntoPlaylistVector(const string &strm3upath, vector<string> &music_list, int &intLinesAdded);

  // MediaPlayer stuff

  // The media_player class handles the specifics of processing announcement queues,
  // switching volumes, volume sliding and changing between XMMS and Linein music.

  media_player MediaPlayer;  
  
  // The MediaPlayer_Play function should be called instead of MediaPlayer.Play() because it does some
  // additional database logging for the benefit of external (Linein) music.
  void MediaPlayer_Play();

  // Handle Announcements 

  // Correct adverts in the database that are incorrectly marked as "about to play" (waiting)
  void CorrectWaitingAnnouncements();

  // doHandleMediaPlayerAnnPlayback is called to wait for MediaPlayer to finish playing announcements.
  // - Call this function after calling MediaPlayer.StartAnnouncementQueuePlay, but only
  //    if that function returned True.    
  bool doHandleMediaPlayerAnnPlayback(ann_playback_status & Status);
  // Variable used by doHandleMediaPlayerAnnPlayback
  long lngLastAnnID; // So that doHandleMediaPlayerAnnPlayback can know when ads finish.
  void WriteErrorsForMissedAds();  

  // Function called only by WriteErrorsForMissedAds
  void WriteErrorsForMissedAds_logmissed(const string strmissed_file, const long lngmissed_count, const DateTime dtmmissed_first, const DateTime dtmmissed_last);

  // MarkAnnounceComplete is only used by doHandleMediaPlayerAnnPlayback();
  void MarkAnnounceComplete(const long lngTZ_Slot);  

  // Volume zones  
  // - Volume zones refer to the ability to adjust the volume for every hour of every day of the week.  
  void volZones();

  // Music History
  void LogLatestMusicMP3Played();

  // Received directory handling (command files)
  void Check_Received();

  // LiveInfo (summary of Player status stored in the Database)
  void WriteLiveInfoSetting(const string strName, const string strValue);
  void doUpdateLiveInfo();

  // Store's OPEN or CLOSED status
  string SemiSonic(); // Return OPEN or CLOSED depending on the current time and store hours
  void StoreClose();  // Close the store (pause playback)
  void StoreOpen();  // Open the store (resume playback)

  // The Player's current status
  struct {
    double curVolume;                // current volume

    double curMusicVolume;           // current music volume
    double curDefaultAnnounceVol;  // current announcement volume
    double curLineInVol; // Current line-in music volume (external music feed)

    double curThisAnnounceVol;

    double curAdjVol;
    string StoreStatus;           // open / closed

    bool blnMediaPaused;              // The user Paused the media playback.
    bool blnMediaStopped;             // The user Stopped the media playback.
  } CurrentStatus;

  // Log XMMS playlist and available music to the database

  // blnMusicLoggingNeeded is set to true when some part of the player logic wants to do the music logging.
  // - the variable is used because the logging operations can take a few minutes, so the player logic waits
  //   until the player is not busy (eg: still starting up) before doing the time-consuming music logging.
  bool blnMusicLoggingNeeded;
  void LogAllMachineMusicToDB();  
  void LogXMMSPlaylistToDB();

  // Log XMMS's status to the database
  void LogXMMSStatusToDB();

  // Function used only by LogXMMSStatusToDB
  void Write_tblPlayerOutput(const string strMessage, const string strMessageDescr);
  
  // RAT code stuff - RAT is a system used for checking command files
  enum enumRAT {ratProblem, ratEncrypted, ratUnEncrypted};
  enumRAT RAT(const string strInputLine);

  // Support for times when the next announcement batch should be started immediatly:
  void do_next_announce_batch_immediately(bool blnimmed);  // Set the state
  bool do_next_announce_batch_immediately(); // Get the state
  bool blndo_next_announce_batch_immediately; // This holds the state.
  
  // Logger object for the player.
  player_logger * Logger;
};

#endif
