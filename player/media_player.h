/***************************************************************************
                          media_player.h  -  description
                             -------------------
    version              : 0.06
    begin                : Mon Jun 23 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *
 * This class is an abstraction - we are no longer XMMS-specific
 * This class also knows the difference between:
 * - Playing adverts (XMMS), playing XMMS music and playing external (line-in) music,
 * - and will adjust volumes accordingly
 *                                                                         *
 ***************************************************************************/

#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H
#define MEDIA_PLAYER_H_VERSION 6 // Meaning 0.06

/**
  *@author David Purdy
  */

#include <string>
#include <deque>
#include <vector>
#include "xmms_controller.h"
#include "check_library_versions.h" // Always last: Check the versions of included libraries.

using namespace rr;

const double dblVolScalePercent = 80;   // Volumes sent to the media_player object will be passed
                                        // to xmms-shell and line-in scaled down to this percent.
                                        // - ie, 100% -> 80% and 50% -> 40%
                                        // - this helps to prevent some distortion in the upper
                                        // volume percentages.

const double MUSIC_VOL_PERCENTAGE = -123; // A special constant passed to
                                          // media_player::PlayAnnouncement which means to
                                          // use the music volume for playing the announcement

enum music_type { // Used internally by media_player to remember what music type is playing now
  xmms, linein
};

// A structure used by media_player to return information to the calling program about the
// current status of announcement playback. It is returned by calls to the
// media_player::AnnPlaybackCheck method.
struct ann_playback_status {
  // These fields are sorted in the order they are most likely to be used...
  bool blnComplete; // Set to false until all the announcements are done playing.
  long lngAnnounceID; // The ID of the announcement - used to tell the difference between individual ads played back...
  bool blnStopped; // Set to true if it is detected that XMMS stopped during playback:
                   //   This can be because of a user or because an announcement finished.
  bool blnUserPaused; // Set to true if it is detected that the user paused the playback
  bool blnUnexpectedErr; // Set to true if an unexpected error occured - eg: XMMS froze
};

// A queue structure for announcements - used internally by xmms_controller
struct TAnnounce {
  string strFilePath; // File location of the announcement
  long lngID;               // Unique ID of the announcement - lets calling progs know what ad is currently playing..
  double dblPercentVol; // Percentage of the regular "announcement" volume to play this ad at.
                              // - Usually 100, can also be MUSIC_VOL_PERCENTAGE
};
typedef deque <TAnnounce> TAnnounceQueue;

const string strlinein_descr = "external music";  // Used when asked for a song title

class media_player {
public:
  media_player();  // Constructor -> Remember to call the "SetXMMSLatency()" method
  ~media_player(); // Destructor

  // Initialization - does not start music playback
  void init();
//  bool init(const string strPlaylistFile, const int intMusicLevel, const int intAnnounceLevel, const int intLineInMusicLevel);

  void SetMusicFadeInLength(const int intMilliseconds); // 1000ms = 1second
  void SetMusicFadeOutLength(const int intMilliseconds); // 1000ms = 1second  

  void QueueAnnouncement(const string strAnnouncePath, const long lngUniqueAnnID, const double dblPercentVol=100);

  int GetAnnouncementQueueLength(); // Fetch the length of the Announcement Queue;

  void StartAnnouncementQueuePlay(ann_playback_status & Status, bool blnWaitForCurrentSongToEnd = false);
                                    // Reset status flags,
                                    // Wait for current XMMS song to end before playing the advert batch?
                                                                                                                 // If music is playing, there is a 2-second fade-out.
  bool AnnPlaybackCheck(ann_playback_status & Status);
                                    // This should be called very fequently during announcement
                                    // playback. It monitors the status of advert playback.
                                    // True is returned while the MediaPlayer object is still playing
                                    // announcements, and False is returned when it is done.
                                    // - This makes the function suitable for a while loop condition.
                                    // After each call, the "Status" parameter is populated
                                    // with the current playback status, which can be
                                    // used to check which announcement is being played back,
                                    // it's position, and if it has been paused or stopped by
                                    // the user, etc.... This function also takes into account
                                    // any XMMS latency.

  void  StopAnnPlayback();          // Stop the announcement playback, kill the internal
                                    // announcement queue, but also DO NOT
                                    // restart music playback (this command stops music playback)

  bool AnnouncementsPlaying();      // Return true if the media player is currently in a "busy playing announcements" state.
  void ResetAnnouncements();        // Clear the announcement  queue, reset any announcement-related variables, and so on.

  // Start music playing again (used after an announcement)
  // - Does not restart playing music, only starts if nothing playing currently.
  void Play(const int intFadeInPeriod=0); // Can also initiate a volume slide if the parameter is given

  void Pause(); // Pause - a resume will continue from within the same place in the music if possible.
  void Stop();   // Stop  - eg we want to stop music playback for the day (and resume in the morning)
  void Resume(); // Continue from where we were after the Stop or Pause.
                           // - The "Play" function above is used more for repopulating the playist and starting
                           // playback if playback is not correct.

  // General maintenance, check that music is still playing if it is meant to be, etc.
  // - Also write any internal class buffers to file that may have been updated in memory.
  // Call this procedure about once a minute (it is not high priority)
  void MaintenanceCheck(); // ** also check for XMMS playback freezes (see commented-out procs.cpp:CheckPlayingFreeze)
                                             // ** also CheckXMMSVolume()
                                             // ** Also hide visible XMMS windows.
  // Change the running settings

  void SetMusicPlaylist(const string strPlaylistFile);
//  bool SetMusicLevel(const int intNewMusicLevel);               // Volumes range from 0-100%
//  bool SetAnnounceLevel(const int intNewAnnounceLevel);
//  bool SetLineInMusicLevel(const int intNewLineInLevel);
  void SetVolumeLevels(const double dblMusicLevel, const double dblAnnounceLevel, const double dblLineInMusicLevel);

  // Return information about MP3s and current playback status.
  string GetSongPath(); // The file path of the current song.
  string GetSongTitle(); // The title of the current song.
  string GetMP3Title(const string strPath); // The title of a random MP3 file - fetch tag details.
  string GetSongTimeStr(); // Return a formatted string showing the current song's time.

  bool IsPlaying(); // Is the playback active?
  bool IsPaused(); // Is the playback paused?

  double GetVolume(); // Get the current music playback volume;
  music_type GetMusicType(); // Get the current music playback type
  string GetMusicTypeStr(); // Get a string description of the current music playback type.

  // For disabling and enabling music playback at various times:
  // (This doesn't actually stop or start music that's already playing or stopped,
  // use Stop() and Play() for that. Those calls also disable/enable music playback )
  bool MusicEnabled() { return blnMusicPlayingEnabled; } // Get status
  void MusicEnabled(const bool blnenabled) { blnMusicPlayingEnabled = blnenabled; }; // Set status

private:
  // An XMMS object to control XMMS
  xmms_controller XMMS;

  // The path of the most recent playlist file
  string strMusicPlaylistFile;

  // Has the playlist file to use been updated lately? (ie has SetMusicPlaylist() been called?)
  bool blnMusicPlaylistUpdated;

  // Music fade-in and fade-out lengths: (default: 5 seconds)
  int intmusic_fadein_length;
  int intmusic_fadeout_length;

  // Volume levels. (0-100%)
  double dblmusic_level;
  double dblannounce_level;
  double dbllinein_level;

  // Current music type being played (xmms or linein)
  music_type current_music_type;

  bool blnMusicPlayingEnabled; // Affected by Play(), Stop() and MusicEnabled().
                               // Also controls the operation of MaintenanceCheck()

  // Functions for controling the linein
  DateTime dtmlinein_play_start; // Used for determining how long the linein has been playing for
  double dbllinein_mostrecent; // The most recently set or retrieved line-in value.

  bool linein_setvol(const double dblvol, bool blnVerbose=true);
  double linein_getvol();

   // The queue of announcements waiting to be played
  TAnnounceQueue AnnounceQueue;
  int intAnnounceTotal;    // Total number of announcments in a currently-playing batch
  int intAnnouncePlayed; // Number of announcements from the current batch that have been started

  // Is the media_player object currently using XMMS for announcement or music playback?
  bool XMMSInUse();
  void VolSlide(const int intSlideLength, const double dblVol1, const double dblVol2=-1);

  // XMMS "random play order" is turned off, the following function is called to
  // keep the music playlist file going, and to randomize the playlist at intervals
  // in a safer way (ie, songs get repeated in succession less)
  void xmms_playlist_file_cycle_random(const int intplaylist_length, const int intplaylist_pos);
  int intPlaylistEntriesCycled; // How many playlist entries have been cycled to the
                                // bottom of the playlist file, by the function above.
  
//  // Debugging function
//  void debug_display_setvol_queue();
};

#endif
