/***************************************************************************
                          media_player.cpp  -  description
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
 *                                                                         *
 ***************************************************************************/

#ifdef __linux__ // the media_player, which uses XMMS, is only usable under linux!

#include "media_player.h"
#include "rr_utils.h"
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include "sys/time.h" // gettimeofday()
#include "string_splitter.h"
#include "temp_dir.h"

// === Constructor, Destructor ===

media_player::media_player(){
  // Constructor - use some default values.
  dblmusic_level = 0;
  dblannounce_level = 0; 
  dbllinein_level = 0;
  blnMusicPlayingEnabled = false;
  dtmlinein_play_start = Now();
  dbllinein_mostrecent = -1;  // This  variable stores the most recently set or retrieved value,
                                            // but we haven't done either yet.
  current_music_type = xmms;
  strMusicPlaylistFile="";
  blnMusicPlaylistUpdated = false; // Music Playlist has not been updated...

  // Set some default values for music fade-in and fade-out lengths: (used when playing announcement batches)
  intmusic_fadein_length   = 5000;  // 5000ms = 5 seconds.
  intmusic_fadeout_length = 5000;

  // Reset variables used for current playing announcement batch
  intAnnounceTotal = 0;
  intAnnouncePlayed = 0;

  // Cycling of the XMMS music playlist:
  intPlaylistEntriesCycled = 0;
}

media_player::~media_player(){
}

// Separate initialization function
void media_player::init() {
  // Check that XMMS is running.
  log_message("Checking XMMS status...");
  
  if (!XMMS.running()) {
    // XMMS is not running. Restart XMMS, wait for it to load.
    log_error("XMMS is not running, restarting XMMS...");
    XMMS.load(true); // Initialize the XMMS controller, AND restart XMMS.
  }
  else {
    log_message("XMMS is running.");
    XMMS.load(false); // Initialize the XMMS controller, but do not restart XMMS.
  }

  // Check if aumix is installed
  if (FileExists("/usr/bin/aumix"))
    log_message("aumix found");
  else
    log_error("aumix not found!");

  // Now fetch the volume levels from XMMS and the linein and store them in the object.
  dblmusic_level = XMMS.getvol();
  dblannounce_level = dblmusic_level;  // Assume for the moment that the announcement level=the music level
  dbllinein_level = linein_getvol();

  // Display the current levels.
  log_message("Current xmms volume = " + itostr(lrint(dblmusic_level)) + "%");
  log_message("Current linein volume  = " + itostr(lrint(dbllinein_level)) + "%");

  // Now that we have the volume levels, guess which type of music is current - XMMS or linein.
  current_music_type = (dbllinein_level>dblmusic_level) ? linein:xmms;
  log_message("Current music type: " + GetMusicTypeStr());

  // Guess whether music is playing now or not.
  blnMusicPlayingEnabled = (dblmusic_level > 0.1) || (dbllinein_level > 0.1);
}

// Added in v6.19 - The music fade-out and fade-out times are now customizable. The default value is 5s

void media_player::SetMusicFadeOutLength(const int intMilliseconds) {
  // Check if the fade-out length  is between 0 and 10 seconds.
  if ((intMilliseconds >= 0) && (intMilliseconds <=10000)) {
    intmusic_fadeout_length = intMilliseconds;
  }
  else {
    rr_throw("Invalid music fade-out length: " + itostr(intMilliseconds) + " (value must be between 0 and 10000)");
  }  
}

void media_player::SetMusicFadeInLength(const int intMilliseconds) {
   // Check if the fade-in length is between 0 and 10 seconds.
  if ((intMilliseconds >= 0) && (intMilliseconds <=10000)) {
    intmusic_fadein_length = intMilliseconds;
  }
  else {
    rr_throw("Invalid music fade-in length: " + itostr(intMilliseconds) + " (value must be between 0 and 10000)");
  }
}


void media_player::QueueAnnouncement(const string strAnnouncePath, const long lngUniqueAnnID, const double dblPercentVol) {
  // Player v6.14.2 added a check: Check if the unique announcement ID or announcment path are already listed!
  TAnnounceQueue::const_iterator ann = AnnounceQueue.begin();
  while (ann != AnnounceQueue.end()) {
    if (ann->lngID == lngUniqueAnnID) {
      rr_throw("Cannot queue the same announcement ID twice in a batch! ID=" + itostr(lngUniqueAnnID));
    }
    else if (ann->strFilePath == strAnnouncePath) {
      rr_throw("Cannot queue the same announcement file twice in a batch! \"" + strAnnouncePath + "\"");
    }
    ++ann;
  }

  // Don't queue the announcement to play if it was found in the check above...
  if (FileExists (strAnnouncePath)) {
    TAnnounce Announce; // Delcare the waiting announcement structure
    Announce.strFilePath=strAnnouncePath; // Store the advert details
    Announce.lngID = lngUniqueAnnID;
    Announce.dblPercentVol = dblPercentVol;
    AnnounceQueue.push_back(Announce);
  } // end if (FileExists)
}

int media_player::GetAnnouncementQueueLength() {
   // Fetch the length of the Announcement Queue;
   int intresult = -1;
   try {
     intresult = AnnounceQueue.size();
   } catch_exceptions;
   return intresult;
}

void media_player::StartAnnouncementQueuePlay(ann_playback_status & Status, bool blnWaitForCurrentSongToEnd) {
  // If music is playing, there is a 5-second fade-out.
  // - Also take into account the possibility of the xmms pre-buffered sound buffer being
  // in use - ie the sound being heard is lagging possibly 3 seconds behind where XMMs reports
  // it to be at - this most directly affects the changing of volumes. So the approach I'm using
  // is to "delay" volume changes by the same amount when we're dealing with XMMS.
  // *Important*: This approach depends implicitely on volume maintenance functions being
  // called every 1/10th of a second.

  // Also - this function does not actually start the announcement playback - the code for this is in AnnPlaybackCheck()

  // Reset a status var that is kept by client code:

  // Clear out the Status variable.
  Status.blnComplete = false;
  Status.lngAnnounceID = -1;
  Status.blnStopped = false;
  Status.blnUserPaused = false;
  Status.blnUnexpectedErr = false;
  
  // Reset the announcement batch-tracking variables
  intAnnouncePlayed = 0;  // A new batch of announcements have started;
  intAnnounceTotal=AnnounceQueue.size(); // The size of this batch

  // Now check if we have announcements to play
  if (AnnounceQueue.size() <= 0) {
    rr_throw("No announcements queued!");
  }

  // Perform the XMMS music playlist "cycle" logic
  // - The current random music playback order has problems, so
  //   XMMS's "random" option is turned off, and this function is called before
  //   each announcement batch to prepare the playlist file for music after
  //   the announcement batch.
  // - See the call to  xmms_playlist_file_cycle_random after this IF block

  // These variables are fetched when we're sure the playlist position isn't
  // going to change during the logic (I don't want to have to skip the
  // next song in case it starts playing during the logic, and is then repeated
  // after the advert)
  int intplaylist_length=-1;  
  int intplaylist_pos=-1;
  
  // Wait for the current song to stop before playing the advert batch?
  if ((GetMusicType() == xmms) && blnWaitForCurrentSongToEnd) {
    // Fetch the playlist lenght & position, used for preparing the next
    // playlist:
    intplaylist_length=XMMS.get_playlist_length();;
    intplaylist_pos=XMMS.get_playlist_pos();
    
    // Wait for the current song to end:
    XMMS.playlist_clear_all_except_current();
    log_message("Waiting for the current song in XMMS to finish playing... (" + itostr(XMMS.get_song_length() - XMMS.get_song_pos()) + "s)");
    while (XMMS.playing()) {
      // Wait 1/2 a second, and check again...
      usleep(1000000/2);
    }

    // XMMS says it is stopped now, but it isn't really. Wait for the sound playback to catch up:
    log_line("[[Waiting " + itostr(XMMS_LATENCY_REPORT) + "ms for xmms-crossfade to finish playing]]");
    usleep(XMMS_LATENCY_REPORT*1000);
    log_line("[[Done waiting]]");
  }
  else {
    // Nope, interrupt the music with a volume slide instead:
    VolSlide(intmusic_fadeout_length, 0); // Spend a period of time (5 seconds?) fading out the music
    // Change in player v 6.17 - Stop XMMS and then clear the playlist. (Swapped the 2 calls around)
    // - // Stop XMMS and clear the XMMS playlist. This is preparation for the first announcement to be played
    XMMS.stop();
    log_line("[[Waiting " + itostr(XMMS_LATENCY_DO) + "ms for xmms-crossfade to catch up]]");
    usleep(XMMS_LATENCY_DO*1000); // Wait now for XMMS to really stop.
    log_line("[[Done waiting]]");

    // Fetch the playlist lenght & position, used for preparing the next
    // playlist:
    intplaylist_length=XMMS.get_playlist_length();;
    intplaylist_pos=XMMS.get_playlist_pos();
  }    
  XMMS.playlist_clear();

  // Now prepare the music playlist to be used after announcement playback:
  if (GetMusicType() == xmms) {
    xmms_playlist_file_cycle_random(intplaylist_length, intplaylist_pos);
  }
}

bool media_player::AnnPlaybackCheck(ann_playback_status & Status) {
  // This should be called very fequently during announcement playback. It monitors the status of
  // advert playback. True is returned while the MediaPlayer object is still playing announcements,
  // and False is returned when it is done.
  // - This makes the function suitable for a while loop condition. After each call, the "Status"
  // parameter is populated with the current playback status, which can be used to check which
  // announcement is being played back, and if it has been paused or stopped by the user,
  // etc.... This function also takes into account any XMMS latency.
  //

  // Static vars used to check if an announcement has changed - we update the
  static string strlast_xmms_media_path = "";
  static long lnglast_announce_id = -1;
  bool blnAddNextAnnouncement = false; // Boolean action flag.

  // First fetch the playlist length
  int intPlaylistLen = XMMS.get_playlist_length();
  
  if (intPlaylistLen == 1) {
    // 1 item in the playlist, a playing announcement or one that has stopped. Check it.
    if (XMMS.playing()) {
      // XMMS is currently playing the listed announcement. Fetch some announcement details.
      Status.lngAnnounceID = lnglast_announce_id; // Return the ID of the most recently started announcement
      Status.blnStopped = false;  // We have determined that XMMS is not stopped      
    }
    else {
      // The listed announcement is not currently playing. Check the reason why the announcement
      // stopped playing.
      if (XMMS.paused()) {
        // The player never pauses XMMS - this must be a user action.
        // (this is a white lie - the player will pause XMMS in response to picking up a pause playback
        // command in the database or in a command file)
        Status.blnUserPaused = true;
        Status.blnStopped = false;  // We have determined that XMMS is not stopped
        return false;
      } else {
        // The playback of the announcement is definitely *stopped* and not *paused*.
        // If in the previous call, XMMS was stopped, then cause the next announcement
        // to be played. If in the previous call, XMMS was still playing, then quit the
        // procedure now (give calling functions a chance to do something when XMMS stops)
        if (Status.blnStopped) {
          // XMMS was stopped on the last call to this function, so in this call we start the next
          // announcement
          blnAddNextAnnouncement = true;          
        }
        else {
          // XMMS was playing on the last call to this function, so just update the "stopped" flag and
          // exit - allow calling code to run "Stopped" logic.
          Status.blnStopped = true;
        }
      }
    }
  } else if (intPlaylistLen == 0) {
    // If at least one announcement from the announcment batch has been started, then there should be
    // 1 item in the playlist, not 0 items!
    if (intAnnouncePlayed > 0) {
      rr_throw("XMMS playlist was cleared during announcement playback!");
    }
    else {
      // Announcements have not yet been started. It's time to add the first announcement
      blnAddNextAnnouncement = true;
    }
  } else if (intPlaylistLen > 1) {
    // Big error! This means that AnnPlaybackCheck was called with a music playlist in XMMS.
    rr_throw("XMMS playlist should have 1 or 0 items listed, but definitely not " + itostr(intPlaylistLen) + " items!");
  } else if (intPlaylistLen < 0) {
    // Some sort of application error.
    rr_throw("Error! The reported playlist length is " + itostr(intPlaylistLen));
  }

  // Now check if it is time to load the next announcement. This was determined
  // earlier in this function
  if (blnAddNextAnnouncement) {
    // -> Here we check if there are any remaining announcements to be played.
    if (AnnounceQueue.size() > 0) {
      // There are more announcements left to play...
      // - Fetch the next announcement's details.
      TAnnounce Announce = AnnounceQueue[0];
      AnnounceQueue.pop_front(); // Now remove it from the queue

      // Check if the file exists
      if (FileExists(Announce.strFilePath)) {

        // Announcement file found. Prepare to play it.

        // Calculate the volume to use. Take into account the special constant which means use
        // the MUSIC VOLUME instead of the usual announcement volume, for this announcement.
        double dblVol = (lrint(Announce.dblPercentVol) == lrint(MUSIC_VOL_PERCENTAGE)) ?
                          dblmusic_level : (dblannounce_level*Announce.dblPercentVol)/100;

        // Has at least one announcement played?
        if (intAnnouncePlayed > 0) {
          // Yes: So wait for xmms-crossfade to finish playing it...
          log_line("[[Waiting " + itostr(XMMS_LATENCY_REPORT) + "ms for xmms-crossfade to finish playing]]");
          usleep(XMMS_LATENCY_REPORT*1000);
          log_line("[[Done waiting]]");            
        }

        // Now change the volume
        XMMS.setvol(dblVol);

        // Also set the linein volume to 0 before each announcement. This is an extra check in
        // case people are playing with linein volumes externally (ie: through the Wizard)
        linein_setvol(0);

        // Queue the file
        XMMS.playlist_clear();
        XMMS.playlist_add_url(Announce.strFilePath);

        // Start playback of the advert
        XMMS.play();

        // Reset the "stopped" flag, XMMS is no longer stopped.
        Status.blnStopped = false;        
        
        // Log a message to say the ad is playing
        ++intAnnouncePlayed; //  Increment the number of announcements played (from the current batch).
        log_message("Playing announcement (" + itostr(intAnnouncePlayed) + "/" + itostr(intAnnounceTotal) + ") : " + GetShortFileName(Announce.strFilePath) + " (id: " + itostr(Announce.lngID) +")");

        // Now remember the ID of the newly started announcement
        lnglast_announce_id = Announce.lngID;

        // Also update the Status variable
        Status.lngAnnounceID = lnglast_announce_id;
      }
      else {
        // Announcement file not found
        log_error("Announcement MP3 " + Announce.strFilePath + "not found!");
      }
    }

    else

    {
      // There are no more announcements left to be played. Restart the music playback
      // if it is currently enabled.
      if (blnMusicPlayingEnabled) {
        // All the announcements have played.

        // Has at least one announcement played?
        if (intAnnouncePlayed > 0) {
          // Yes: So wait for xmms-crossfade to finish playing it...
          log_line("[[Waiting " + itostr(XMMS_LATENCY_REPORT) + "ms for xmms-crossfade to finish playing]]");
          usleep(XMMS_LATENCY_REPORT*1000);
          log_line("[[Done waiting]]");            
        }

        // Log that the music will now resume.
        log_message("Announcements done, music playback resumed.");

        // Sleep a bit, give XMMS a chance to

        // Also this will help to avoid any tiny truncation of announcement playback that may still remain.
//        usleep(1000000/2); // Sleep half a second
        Play(intmusic_fadein_length); // Start music playback and take 5 seconds to slide the music playback volume to full.

        // Reset "current announcement batch" counters
        intAnnounceTotal = 0;
        intAnnouncePlayed = 0;

        return false; // This means there are no more ads remaining, so loops calling this
                            // function can terminate.
      }
      else {
        // Music playing is not enabled at this point. Return false also because there are no more ads
        // to play
        return false;
      }
    }
  }
  return true; // Ads are still busy playing announcements if the execution reaches this point.
}

bool media_player::AnnouncementsPlaying() {
  // Return true if the media player is currently in "busy playing announcements" state.
  return intAnnouncePlayed > 0;
}

void media_player::ResetAnnouncements() {
  // Clear the announcement  queue, reset any announcement-related variables, and so on.
  intAnnounceTotal = 0;
  intAnnouncePlayed = 0;
  AnnounceQueue.clear();
}

// Start music playing again (used after an announcement)
// - Does not restart playing music, only starts if nothing playing currently.
void media_player::Play(const int intFadeInPeriod) {
  // intFadeInPeriod determines if there will be a music volume slide (from the current volume to the music vol)
  // and how long it will be before the volume is reached.

  // Open the playlist file, read the first line and see if it is "linein"
  // - This is the requirement for linein music - otherwise, use a regular XMMS music playlist.
  music_type NewMusicType;
  ifstream inFile(strMusicPlaylistFile.c_str());
  if (!inFile) {
    // Unable to open file - return false
    rr_throw("Unable to open playlist file " + strMusicPlaylistFile);
  }
  else {
    char ch_FileLine[2048] = "";
    if (inFile.getline (ch_FileLine, 2047)) {
      inFile.close(); // Now that we have read the first line of the file, close it.
      string strLine= ch_FileLine;
      NewMusicType = (trim(StringToLower(strLine))=="linein")?linein:xmms;
    }
    else {
      // Could not read a line from the playlist - possibly the playlist is empty?
      log_error("Error encountered reading the first playlist line! " + strMusicPlaylistFile);
    }
  }

  // We now have the new "music type" to be playing. Change over.
  if (NewMusicType != current_music_type) {
    log_message(string("Changing music type to ") + (NewMusicType==xmms ? "XMMS" : "LineIn"));

    // We immediately stop the old music type and set the new music type's volume level to 0;
    XMMS.stop(); // Whether xmms music is starting or stopping, this is a good first step.
    XMMS.setvol(0);
    XMMS.playlist_clear(); // Clear the xmms playlist also. A special request of Arthur's
    linein_setvol(0);

    blnMusicPlaylistUpdated=true; // If the music type changes between LineIn and XMMS then
                                  // this is taken as granted (that the music playlist has changed)
    // Now change over the internal music type;
    current_music_type = NewMusicType;
  }

  double dblVolSlideStart = GetVolume(); // We slide the music volume from the old volume to the correct volume.
                                         // - however, sometimes we reset the "start" volume slide value to 0.

  // The "new" music type has been set. Now build up the new playlist (if we're using XMMS)
  if (current_music_type==xmms) {
    // Current music type is xmms. Build up the new playlist.

    // Check if XMMS is currently playing, and if it is, what the current song's path is
    // - We don't want to interrupt XMMS's playback if the current song in the playlist
    // is in the new playlist.
    bool blnXMMSPlaying = XMMS.playing(); // Is XMMS playing at the moment?
    bool blnPlaylistCleared = false; // XMMS is stopped when the playing playist item is cleared

    // If XMMS is currently NOT playing, then also quickly queue an immediate 0 setvol - XMMS not currently playing
    // can mean that announcements just finished playing - so the current volume in XMMS is not acceptible for sliding from
    // during music playback.
    if (!blnXMMSPlaying) {
      // XMMS is not currently playing - the volume goes to 0 at the start of the volume slide
      dblVolSlideStart = 0;
      // Set the XMMS volume here to 0. This is so that we don't accidentally
      // start playing XMMS at the previous non-zero volume for an moment, before sliding
      // from 0 upwards.
      XMMS.setvol(0);
    }

    // Was either 1) The music playlist updated, or
    //            2) an announcement just playing? (1 entry in the playlist)

    bool blnFirstPreserved = false; // Set to true if the first playlist entry was preserved.
    
    if ((blnMusicPlaylistUpdated) || (XMMS.get_playlist_length() <= 1) ) {
      string strMP3Path = trim(XMMS.get_song_file_path());
      if (strMP3Path != "" && FindTextInFile(strMP3Path, strMusicPlaylistFile)) {
        // Current XMMS MP3 is in the new playlist. So clear
        // off all the XMMS playlist items *except* for the current item.
        XMMS.playlist_clear_all_except_current(); // Do not interrupt the current song
        blnFirstPreserved = true; // Later we remove the matching line from the playlist.
      } // end if
      else {
        // Current XMMS MP3 is NOT in the new playlist. So clear
        // of all the XMMS playlist items *including* the current item.
        blnPlaylistCleared = true; // If xmms is playing, clearing the playlist will stop playback
        XMMS.playlist_clear(); // Clear the playlist, interrupt the current song also!
        // Only bother to log a warning message if playback is actually happening
        if (blnXMMSPlaying) {
          log_message("Current song is not in new playlist, interrupting playback.");
        } // end if
      } // end else

      // Now that the playlist is cleared out, load the new playlist file.
      // - The "true" means overwrite the default XMMS playlist file that is loaded
      // by XMMS when it starts up. This playlist should have the most recent profile's
      // music in it.
      XMMS.playlist_load(strMusicPlaylistFile, true);

      // Next, if we didn't remove the current song (ie, it's listed twice in the playlist at the moment),
      // remove the 2nd occurance now (but ignore any others after the first match)
      if (blnFirstPreserved) {
        string strFirstFile=XMMS.get_playlist_file(0);
        int intplaylist_length = XMMS.get_playlist_length();

        bool blnfound = false;
        int intpos=1; // Current position in the playlist
        while (intpos < intplaylist_length && !blnfound) {
          if (XMMS.get_playlist_file(intpos) == strFirstFile) {
            // We found the match, so remove it.
            XMMS.playlist_delete(intpos);
            blnfound=true;
          }
          ++intpos; // Next entry.
        }

        // Did we find a match?
        if (!blnfound) rr_throw("Did not find an XMMS entry I expected to be there!");
      }

      // Now check the playlist length - should be at least 10 mp3s playing
      int intplaylist_len = XMMS.get_playlist_length();
      if (intplaylist_len < 10) {
        log_warning("The new playlist (" + strMusicPlaylistFile + ") only has " + itostr(intplaylist_len) + " song(s)!");
      } // end if

      // The new music playlist is now loaded.
      blnMusicPlaylistUpdated = false;

      // Also, the new playlist has not been "cycled" yet, ie,
      // its not time to re-randomize it (we do this occasionally, as
      // XMMSs random playback order is now removed).
      intPlaylistEntriesCycled=0;
    } // end if

    // Now start XMMS
    XMMS.play();            // If XMMS is already playing, this function will not restart it
                            // (the xmms_controller class is semi-intelligent)
  }
  else {
    // Current music type is linein. So we stop XMMS.
    // It doesn't hurt to stop again if XMMS already is stopped...
    XMMS.stop();
  }

  // Now the new music is set up (ie, a new playlist for XMMS if the music type is XMMS),
  // we fade to the new volume level. Usually this entails sliding from 0, but this can vary
  // depending on the actual level of the sound device when this function is called (usually
  // the volume will be 0

  // Initiate a 5-second volume slide to the new music type's level.
  // - Usually we start at the current device level and slide up or down to the required level, but
  // sometimes we start the volume slide at 0 (eg: the sound volume is at announcement level)

  // Sleep a bit, wait for XMMS to respond to the "PLAY" command, before sliding the volume:
  log_line("[[Waiting " + itostr(XMMS_LATENCY_DO) + "ms for xmms-crossfade to catch up]]");
  usleep(XMMS_LATENCY_DO*1000); // Wait now for XMMS to really stop.
  log_line("[[Done waiting]]");

  VolSlide(intFadeInPeriod, dblVolSlideStart, (current_music_type==xmms)?dblmusic_level:dbllinein_level); // specified milliseconds, and the volume.

  blnMusicPlayingEnabled = true; // Music playing is now turned on, because this function was called
  ResetAnnouncements();          // If this function is called while we're in a "playing announcements" state, then clear
                                 // the announcement  queue, reset any announcement-related variables, and so on.
}

void media_player::Pause() {
  // Grab the current music status and then stop the playback music.

  // Player version 6.15 - Pause and Resume temporarily have the same effect as Stop and Play
  // - there is no need to properly implement this yet.
  log_message("Pause now behaves like Stop()...");
  Stop();
////
////
////  try {
////    // Grab the current playback status.
////    PauseStatus.blnPaused = true; // We are now in a paused state.
////    PauseStatus.ActivePlayer = XMMSInUse() ? xmms:linein; // Which playback device to switch to when we resume.
////    PauseStatus.intOldVol = GetVolume();
////
////    // If we are currently playing from XMMS, then save the XMMS status
////    if (PauseStatus.ActivePlayer==xmms) {
////      // v6.15 - telling XMMS to pause, not bothering to save it's entire state.
//////      XMMS.state_save(&PauseStatus.XMMS_state, getcwd() + "/pause_playlist.m3u");
////      if (!XMMS.paused()) {
////        XMMS.pause();
////      }
////    }
////    // Now stop the playback.
////    blnMusicPlayingEnabled = false; // Music playing is now turned off
////
////    // Stop the playback devices
//////    XMMS.pause();
//////    XMMS.setvol(0);
////    linein_setvol(0);
////    return true;
////  } catch_exceptions;
////  return false;
}

// Stop the music - eg we want to stop music playback for the day (and resume in the morning)
void media_player::Stop() {
  // Update internal variables;
  blnMusicPlayingEnabled = false; // Music playing is now turned off

  // Turn off all the playback devices
  XMMS.stop();
//    XMMS.setvol(0);
  linein_setvol(0);
}

// Resume playback after a pause (or stop) - eg the user paused in the middle of announcement playback...
void media_player::Resume() {
  // Player version 6.15 - Pause and Resume temporarily have the same effect as Stop and Play
  // - there is no need to properly implement this yet.

  log_message("Resume() now behaves like Play()...");
  Play();
}

// General maintenance, check that music is still playing if it is meant to be, etc.
// - Also write any internal class buffers to file that may have been updated in memory.
// Call this procedure about once a minute (it is not high priority)
void media_player::MaintenanceCheck() { // ** also check for XMMS playback freezes (see commented-out procs.cpp:CheckPlayingFreeze)
  // This function does some background checks on the music playing.
  // ** also CheckXMMSVolume()
  // ** Also hide visible XMMS windows.
  // ** Also set shuffle and repeat modes.
  // ** Also write mp3 tags to text file if the internal collection has changed.

  // Allow the XMMS controller object to flush any mp3 tag updates to disk.
  XMMS.maintenance_check();

  // Set shuffle off and repeat off, and hide xmms also
  XMMS.setshuffle(false);
  XMMS.setrepeat(false);
  XMMS.hide_windows();

  // Check if XMMS is running
  if (!XMMS.running()) {
    log_error("XMMS not running, attempting to start up XMMS...");
    XMMS.load(true);

    // Complain if XMMS is still not loaded
    // This is a logged message, not an error: The player will be started
    // usually before X has finished starting - so for a while it will
    // not be able to start up XMMS.
    if (!XMMS.running()) {
      if (GetUpTime() > (10*60)) {
        // Machine has been up for 10 minutes and we're still not
        // able to detect XMMS running. Something is wrong here.
        log_error("XMMS should be running by now!");
      }
      else {
        // Machine has been up for less than 10 minutes - it's ok if
        // we're not able to run XMMS yet - just keep trying.
        log_message("XMMS is not running! Will continue to check");
      }
    }
  }

  // Check the XMMS volume
  bool blnIncorrect=false;
  double dblCorrectVol=-1;
  double dblCurrXMMSVol = XMMS.getvol();

  if (XMMS.running() && blnMusicPlayingEnabled && current_music_type==xmms) {
    // Music is meant to be playing now, and the current music type is XMMS.
    // - volume should be at the correct level.
    if (abs(dblCurrXMMSVol - dblmusic_level) > 5) {
      // XMMS's volume is more than 5% off. Correct it.
      dblCorrectVol = dblmusic_level;
      blnIncorrect = true;
    }
  } //else {
    // Music is not meant to be playing now, or the current music type is not XMMS.
    // Change in 6.15 - XMMS's volume is not adjusted unless XMMS is actually playing
    //  - if XMMS is currently stopped or paused, the volume is not affected...

//      // - Volume should be 0.
//      if (intCurrXMMSVol > 5) {
//        // XMMs' volume is more than 5% off Correct it.
//        intCorrectVol = 0;
//        blnIncorrect = true;
//      }
//    }

  // If we have determined that XMMS's volume is incorrect, then fix the volume
  if (blnIncorrect) {
    log_message("Correcting XMMS volume from " + itostr(lrint(dblCurrXMMSVol)) + "% to " + itostr(lrint(dblCorrectVol)) + "%");
    XMMS.setvol(dblCorrectVol);
  }

  // Check that XMMS is playing/stopped correctly, depending on the current music playing state
  blnIncorrect=false;
  bool blnPlaying = XMMS.playing();

  if (XMMS.running() && blnMusicPlayingEnabled && current_music_type==xmms) {
    // Current music type is XMMS, and music is meant to be playing at the moment.
    // - so XMMS must be playing.
    blnIncorrect = !blnPlaying;
  } else {
    // Current music type is not XMMS, or music is not meant to be playing at the moment
    // - so XMMS must not be playing
    blnIncorrect = blnPlaying; //
  }

  // Now adjust XMMS's playing state if we found that is is incorrect
  if (blnIncorrect) {

    if (blnPlaying) {
      // It is incorrect to be playing, so stop the music playback
      log_error("XMMS is not meant to be playing! Stopping XMMS...");
      XMMS.stop();
    } else {
      // It is incorrect to not be playing, so start the music playback.
      log_message("XMMS is meant to be playing! Restarting music playback... ");
      XMMS.playlist_clear();
      XMMS.playlist_load(strMusicPlaylistFile);
      XMMS.setvol(dblmusic_level);
      XMMS.play();
    }
  }

  // Now check the linein volume
  blnIncorrect=false;
  double dblCurrLineInVol = linein_getvol();
  dblCorrectVol = -1;

  if (blnMusicPlayingEnabled && current_music_type==linein) {
    // Linein volume should be set correctly
    if (abs(dblCurrLineInVol - dbllinein_level) > 5) {
      dblCorrectVol = dbllinein_level;
      blnIncorrect = true;
    }
  } else {
    // Linein should be muted
    if (dblCurrLineInVol > 5) {
      dblCorrectVol = 0;
      blnIncorrect = true;
    }
  }

  // Correct the LineIn vol if necessary
  if (blnIncorrect) {
    log_message("Correcting linein volume from " + itostr(lrint(dblCurrLineInVol)) + "% to " + itostr(lrint(dblCorrectVol)) + "%...");
    linein_setvol(dblCorrectVol);
  }

  // Check that XMMS is not frozen (ie, it says that it is playing, but the music is not progressing)
  // - We do this by comparing the state of XMMS between calls.
  static string strlast_song_name = "";
  static long lnglast_song_pos = -1;
  static DateTime dtmlast_xmms_check=datetime_error;

  if (XMMS.playing()) {
    // XMMS reports it is currently playing. Has some time elapsed (2 seconds) elapsed since the last
    // check?
    if (abs(Now()-dtmlast_xmms_check) >= 2) {
      // At least 2 seconds have passed since the last XMMS check by this function.
      // Now fetch and check the current XMMS playing status.
      long lngXMMSSongPos   = XMMS.get_song_pos_ms();
      string strXMMSSongPath = XMMS.get_song_file_path();

      if ((lngXMMSSongPos == lnglast_song_pos) && (strXMMSSongPath == strlast_song_name)) {
        // XMMS reports it is playing, and at least 2 seconds have elapsed since the last
        // check, but the mp3 and song position have not changed!
        // - Bad luck XMMS, you'r going down.
        log_error("XMMS says it is playing but it in fact frozen. Restarting XMMS (Please check the sound daemon)");
        XMMS.load(true);
      }
      // Check done. Now update the "state remembering" varialbles.
      lnglast_song_pos = lngXMMSSongPos;
      strlast_song_name = strXMMSSongPath;
      dtmlast_xmms_check=Now();
    }
  } else {
    // XMMS reports it is not currently playing. Reset the checking variables.
    lnglast_song_pos = -1;
    strlast_song_name = "";
    dtmlast_xmms_check=datetime_error;
  }
}

// Change the running settings
void media_player::SetMusicPlaylist(const string strPlaylistFile) {
  // This function's design has changed: All it does is actually "set" the music playlist to
  // be used for music playback by the media_player class. It does not affect the current
  // XMMS playback status in any way though. Run this function, then run PlayMusic.
  if (FileExists(strPlaylistFile)) {
    // Playlist file exists. Set it as the current music playlist.
    strMusicPlaylistFile = strPlaylistFile;
    blnMusicPlaylistUpdated = true; // Music Playlist was updated. It will be loaded the next time Play() is called
  }
  else {
    // Playlist file does not exist. Log an error
    rr_throw("Playlist file not found! " + strPlaylistFile);
  }
}

void media_player::SetVolumeLevels(const double dblMusicLevel, const double dblAnnounceLevel, const double dblLineInMusicLevel) {
  // Update internal variables - take into account the global volume scaling (ie, we don't
  // want some sound levels to get to 100% so they are scaled down say to 80% of the original volume.

  dblmusic_level = (dblMusicLevel * dblVolScalePercent) / 100;
  dblannounce_level = (dblAnnounceLevel * dblVolScalePercent) / 100;
  dbllinein_level = (dblLineInMusicLevel * dblVolScalePercent) / 100;

  // Additionally - only actually update the device levels if music playing is currently enabled,
  // ie, the playback has not been stopped or paused.

  // Check which music is active - line-in or XMMS?
  switch (current_music_type) {
    case xmms:
       // XMMS? Update the XMMS volume
       if (blnMusicPlayingEnabled) {
         XMMS.setvol(dblmusic_level);
         linein_setvol(0); // Mute
       }
       break;
    case linein:
      // Line-in external music? Update the Line-in music volume.
      if (blnMusicPlayingEnabled) {
        XMMS.setvol(0); // Mute
        linein_setvol(dbllinein_level);
      }
      break;
    default: rr_throw("Unknown music type! not [xmms] or [linein]");
  }
}

// Return information about MP3s and current playback status.
string media_player::GetSongPath() {
  // The file path of the current song. If XMMS is not the active music player then return ""
  if (current_music_type == xmms) {
    return XMMS.get_song_file_path();
  }
  return "";
}

string media_player::GetSongTitle() {
  // The title of the current song.
  switch(current_music_type) {
    case xmms:
      return XMMS.get_song_title();
    case linein:
      return strlinein_descr;
      break;
  }
  rr_throw("Unknown current music type!");  
}

string media_player::GetMP3Title(const string strPath) {
   // The title of a random MP3 file - fetch tag details.
   if (StringToLower(trim(strPath)) == "linein")
     // If the "mp3" that was listed in a playlist file has "linein" in it, then this is a special marker,
     // not actually an mp3 file location.
     return strlinein_descr;
   else
     return XMMS.get_mp3_title(strPath);
}

string media_player::GetSongTimeStr() {
  // Return a formatted string showing the current song's time.
  switch(current_music_type) {
    case xmms:
      return XMMS.get_song_time_str();
      break;
    case linein:
      if (dtmlinein_play_start != datetime_error) {
        // Calculate how long the line-in has been playing, convert this to "HH:NN:SS"
        char strTemp[10];
        string strtime_str;
        long lngseconds = Now() - dtmlinein_play_start;

        // Calculate seconds
        sprintf(strTemp, "%02d", int(lngseconds%60));
        strtime_str = string(":")+ strTemp;

        // Calculate minutes
        lngseconds /= 60;
        sprintf(strTemp, "%02d", int(lngseconds%60));
        strtime_str = string(":") + strTemp + strtime_str;

        // Calculate hours
        lngseconds /= 60;
        sprintf(strTemp, "%02d", int(lngseconds));
        strtime_str = strTemp + strtime_str;

        // Now we have the full time string. Return it
        return strtime_str;
      } else {
        // Something is wrong, linein is being used to play music, but the play-time calculation variable
        // is not initialised correctly.
        rr_throw("Error occured calculating LineIn time playing!");
      }
      break;
    default:
      rr_throw("Unknown current music type!");
  }
}

bool media_player::IsPlaying() {
  // Is the playback active?
  switch (current_music_type) {
    case xmms:
      return XMMS.playing();
      break;
    case linein:
      return linein_getvol() >= 0.5; // Assume that line-in is playing if the volume is greater than 0.5
      break;
    default:
      rr_throw("Unknown music type!");
      break;
  }
  return false;
}

bool media_player::IsPaused() {
  // Is the playback paused?
  switch(current_music_type) {
    case xmms:
      return XMMS.paused();
      break;
    case linein:
      return false; // My arbitrary convention - Linein is either Playing or not Playing, but never Paused.
      break;
    default:
      rr_throw("Unknown current music type!");
  }
  return false;
}

double media_player::GetVolume() {
  // Get the current music playback volume;
  switch (current_music_type) {
    case xmms:
      return XMMS.getvol();
      break;
    case linein:
      return linein_getvol();
      break;
    default:
      rr_throw("Unknown current music type!");
  }

  return -1; // Error!
}

music_type media_player::GetMusicType() {
  // Return the current music playback type
  return current_music_type;
}

string media_player::GetMusicTypeStr() {
  // Return the current music playback type                             51 - mail - pb 447 - tool -
  string strret = "ERROR";
  try {
    switch (current_music_type) {
      case xmms:
        strret = "XMMS";
        break;
      case linein:
        strret = strlinein_descr;
        break;
      default:
        rr_throw("Unknown current music type!");
    }
  } catch_exceptions;
  return strret;
}

// Functions for controlling the linein volume level
bool media_player::linein_setvol(const double dblvol, bool blnVerbose) {
  // Keep track of how long line-in music has been playing for.
  // Keep track of how long line-in music has been playing for.
  if ((int)dbllinein_mostrecent != -1) {
    if ((int)dbllinein_mostrecent==0 && (int)dblvol>0) {
      // The linein was originally set to silent and has now been started up.
      dtmlinein_play_start = Now(); // Time playing is measured from now.
    }
    else if ((int)dbllinein_mostrecent>0 && (int)dblvol==0) {
      // The linein was originally playing and has now been muted.
      dtmlinein_play_start = datetime_error; // Don't use this variable if the linein is not playing!
    }
  }

  // Only log a "volume changed" message if the volume being set here is different
  // to the level that was last set or read.
  if (lrint(dbllinein_mostrecent) != lrint(dblvol)) {
    if (blnVerbose) {
      log_message(string("LineIn volume set to ") + itostr(lrint(dblvol)) + "%");
    }
  }
  dbllinein_mostrecent = dblvol;

  return system(string(string("/usr/bin/aumix -l ") + itostr(lrint(dblvol))).c_str())==0;
}

double media_player::linein_getvol() {
  // Use an "aumix -q" call combined with a "grep" to fetch the linein level
  FILE *fp;
  char line[130];   /* line of data from unix command*/
  double dblReturnVol = -1; // Value read in from aumix

  string ToRun = "aumix -q | grep \"line \"";
  fp = popen(ToRun.c_str(), "r");   /* Issue the command. */

  /* Read a line			*/
  if (fgets(line, sizeof line, fp))
  {
    // Decode the line
    string strLine = substr(line, 5);
    string_splitter split(strLine, ", ");

    string strLeft  = split;
    string strRight = split;

    // Check that the extracted values are valid
    if (IsInt(strLeft) && IsInt(strRight)) {
      // Values are read in from aumix
      dblReturnVol = (double)(strtoi(strLeft) + strtoi(strRight)) / 2;
      // Also update an internal variable which remembers the most recently known linein value.
      dbllinein_mostrecent=dblReturnVol;
    }
    else {
      // Values extracted from aumix are not numeric!
      rr_throw("Bad linein values read from aumix!");
    }
  }
  pclose(fp);
  return dblReturnVol;
}

// Function to return whether the media_player class is currently using XMMS for music or announcement playback
// (alternatively it could currently just be using linein for music output) - this affects the way in which
// volume changes are queued
bool media_player::XMMSInUse() {
  // If 1) music playback is enabled and the current music type is XMMS, or announcements have started playing (for
  // a *current* announcement batch), then XMMS is being used at the moment.
  // - In all other cases we are using the linein, or there is no sound playback at all (eg: it is now outsid of store hours)
  return (blnMusicPlayingEnabled && current_music_type == xmms) || intAnnouncePlayed > 0;
}

void media_player::VolSlide(const int intSlideLength, const double dblVol1, const double dblVol2) {
  // Initiate a slide over the next [intFadeLength] milliseconds. This can be for XMMS or LineIn
  // (delay it by the size of the pre-gen sound buffer if we're using XMMS for music,
  // otherwise if we're using line-in, the commands are not delayed before being run)
  //
  // Note: Normally this function is called with 2 parameters - the slide length and the dest volume.
  //  - In this case, intVol1 will contain the destination volume. However, sometimes the
  // calling function will call this function with 3 parameters - then intVol1 contains the slide from volume,
  // and intVol2 contains the destination volume.

  double dblStartVol=-1;
  double dblDestVol=-1;

  if ((int)dblVol2 == -1)  {
    // Volume parameter 2 is not provided, so start from the current volume
    dblStartVol = GetVolume();
    dblDestVol = dblVol1;
  }
  else {
    // Volume parameter 2 is provided, meaning the starting volume is provided.
    dblStartVol = dblVol1;
    dblDestVol = dblVol2;
  }

  // Quit if the source volume is the same as the destination volume. No point in causing a delay where no
  // volume changes happen
  // Player 6.17: Corrected the logic for floating point comparisons:
  if (lrint(dblStartVol) == lrint(dblDestVol)) return;
  
  log_message("Starting a " + itostr(intSlideLength) + "ms volume slide from " + itostr(lrint(dblStartVol)) + "% to " + itostr(lrint(dblDestVol)) + "%...");

  // Music fade-outs - reduce the volume from the current music volume (whether it is XMMS or LineIn) to
  // 0. We break the period up into 1/5'ths of a second interval and queue a volume change for each interval.
  int inttotal_intervals = intSlideLength/200; // 200ms = 1/5'th of a second.
  if (inttotal_intervals <=0) {
    inttotal_intervals = 1; // Avoid division by zero and other possible nasties.
  }

  double dblPrevVol = dblStartVol;
  double dblNewVol = -1;

  for (int int_num=1; int_num <= inttotal_intervals; int_num++) {
    // An extra parameter - verbose - log when the final volume change has executed, do not log all of the
    // intermediate volume changes.
    dblNewVol = dblStartVol + ((dblDestVol - dblStartVol)*int_num) / inttotal_intervals;

    // Sleep for 1/5th of a second:
    usleep(1000000/5);

    // Now set the volume:     
    if (dblPrevVol != dblNewVol) { // Don't set the volume more than onece for a %.
      dblPrevVol = dblNewVol;    
      switch(current_music_type) {
        case xmms:
          XMMS.setvol(lrint(dblNewVol), ((int)dblNewVol == (int)dblDestVol)?true:false);
          break;
        case linein:
          linein_setvol(lrint(dblNewVol), ((int)dblNewVol == (int)dblDestVol)?true:false);        
          break;
        default: rr_throw("Unknown music type! not [xmms] or [linein]");
      }
    }
  }
}

void media_player::xmms_playlist_file_cycle_random(const int intplaylist_length, const int intplaylist_pos) {
  // XMMS's "random playback" order is turned off. It's not good enough, as
  // the playlist is reset for announcement playback, and songs are that played
  // shortly before the announcement batch are often repeated.
  // So the media_player now turns XMMS's random function off, and uses this function
  // (before announcement batches) to reorder the the playlist file. The updated
  // playlist file is reloaded after announcement playback.

  // note: intplaylist_pos is 0-based, which is why we add 1 in many calculations.
  
  // 1) Are we currently using XMMS to play music?
  if (current_music_type != xmms) return; // Don't do playlist logic if not.

  // 2) Does the playlist file exist?
  if (!FileExists(strMusicPlaylistFile)) {
    rr_throw("Playlist file not found! " + strMusicPlaylistFile);
  }

  // 3) Count the playlist file lines. Should be the exact same length
  //    as the playlist length in XMMS.
  int intfile_lines = CountFileLines(strMusicPlaylistFile);
  
  if (intfile_lines != intplaylist_length) {
    rr_throw("XMMS playlist has " + itostr(intplaylist_length) + " entries, but playlist file has " + itostr(intfile_lines) + " lines!");
  }
  
  // Move lines from the top of the playlist file to the end. (Playback
  // begins at the top of the file)

  // Create a temporary directory to work under:
  temp_dir work_dir("new_playlist");

  // Create a new playlist file, with entries *after* the current playlist position
  // - Build the command:
  string strcmd = (string) "/bin/bash -c \"tail -n " + itostr(intplaylist_length - intplaylist_pos - 1) + " \\\"" + strMusicPlaylistFile + "\\\" > " + (string)work_dir + "playlist.m3u" + "\"";

  // - Run & check the return
  int intret = system(strcmd.c_str());

  if (intret != 0) {
    rr_throw("I had a problem cycling the playlist file!");
  }
    
  // - Count the lines in the file:
  int intnew_count=CountFileLines((string)work_dir + "playlist.m3u");
  if (intnew_count != intplaylist_length - intplaylist_pos - 1) {
    rr_throw("The intermediate playlist should have " + itostr(intplaylist_pos + 1) + " lines, but " + itostr(intnew_count) + " were found!");
  }

  // Append the other lines (originally at the top of the file):
  strcmd = "/bin/bash -c \"head -n " + itostr(intplaylist_pos + 1) + " \\\"" + strMusicPlaylistFile + "\\\" >> " + (string)work_dir + "playlist.m3u" + "\"";
  intret = system(strcmd.c_str());

  if (intret != 0) {
    rr_throw("I had a problem cycling the playlist file!");
  }

  // - Count the lines in the file now:
  intnew_count=CountFileLines((string)work_dir + "playlist.m3u");
  if (intnew_count != intplaylist_length) {
    rr_throw("The intermediate playlist should have " + itostr(intplaylist_length) + " lines, but " + itostr(intnew_count) + " were found!");
  }

  // Increase the counter of lines that have been "cycled" to
  // the bottom of the file:
  intPlaylistEntriesCycled+=intplaylist_pos+1;

  // Time to randomize the playlist?
  if (intPlaylistEntriesCycled >= intplaylist_length) {
    // Yes: Do so here:
    // - This command generates playlist.m3u.new, which is a random version of playlist.m3u
    strcmd = "/bin/bash -c \"cat \\\"" + (string)work_dir + "playlist.m3u\\\" | awk 'BEGIN{srand()} {print rand()\\\"|\\\"\\$0}' | sort | awk -F '|' '{print \\$2}' > " + (string)work_dir + "playlist.m3u.new" + "\"";
    intret = system(strcmd.c_str());

    if (intret != 0) {
      rr_throw("I had a problem randomizing the playlist file!");
    }

    // Count the lines in the file:
    int intnew_file_lines=CountFileLines(string_to_unix_filename((string)work_dir + "playlist.m3u.new"));
    if (intnew_file_lines != intplaylist_length) {
      rr_throw("The randomized playlist file has " + itostr(intnew_file_lines) + " lines instead of the expected " + itostr(intplaylist_length) + "!");
    }

    // No problem, so move the new file over our original temporary one:
    mv((string)work_dir + "playlist.m3u.new", (string)work_dir + "playlist.m3u");
        
    // And reset the counter also:
    intPlaylistEntriesCycled = 0;
  }

  // Logic done. Move the temporary playlist file over the original. It will
  // start playing from line 1 downwards, the next time XMMSs playlist is loaded
  // (after the advert batch)
  mv((string)work_dir + "playlist.m3u", strMusicPlaylistFile);
}

#endif // END: #ifdef __linux__
