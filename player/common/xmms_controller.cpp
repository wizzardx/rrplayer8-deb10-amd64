/***************************************************************************
                          xmms_controller.cpp  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifdef __linux__ // XMMS support is only available under Linux!

#include "xmms_controller.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h> // sleep() functon
#include "xmmsctrl.h" // XMMS API functions
#include "glib.h" // Needed to access some "xmmsctrl" functions
#include "string_splitter.h"
#include "exception.h"
#include "file.h"
#include "my_string.h"
#include "testing.h"

xmms_controller::xmms_controller() {
  intsession = 0;
}

xmms_controller::xmms_controller(const int intsession_arg){
  intsession = intsession_arg;








}

xmms_controller::~xmms_controller(){
}

int xmms_controller::get_playlist_length(){
  return xmms_remote_get_playlist_length(intsession);
}

int xmms_controller::get_song_length() {
  // Returns the current song length in seconds
  return get_song_length_ms() / 1000;
}

int xmms_controller::get_song_length_ms() {
  // Returns the current song length in milliseconds
  int intplaylist_pos = xmms_remote_get_playlist_pos(intsession);
  int intplaylist_len = xmms_remote_get_playlist_length(intsession);
  // Check if the playlist pos is valid (must be >= 0 and < PL_length)
  if (intplaylist_pos >= 0 && intplaylist_pos < intplaylist_len) {
    // xmms returns the song length to us in milliseconds, convert to seconds.
    return  xmms_remote_get_playlist_time(intsession, xmms_remote_get_playlist_pos(intsession));
  }
  else {
    my_throw("Bad playlist position!");
  }
}

int xmms_controller::get_playlist_pos() {
  // Return the current playlist position (0-based)
  return xmms_remote_get_playlist_pos(intsession);
}

int xmms_controller::get_song_pos() {
  // Value returned is in milleseconds. Convert to seconds.
  return xmms_remote_get_output_time(intsession)/1000;
}



int xmms_controller::get_song_pos_ms() {
  // Get song position in milliseconds
  return xmms_remote_get_output_time(intsession);
}

string xmms_controller::get_song_time_str() {
  // Retrieve the Current song position and make an attempt to format it.
  // The expected format is "00:00.00"

  long lngoutput_time = xmms_remote_get_output_time(intsession) / 10; // Returns 1000'ths, we start with 100'ths
  string strtime_str = "";

  // Calculate hundredths of seconds
  char strTemp[10];
  sprintf(strTemp, "%02d", int(lngoutput_time % 100));
  strtime_str = string(".") + strTemp;

  // Calculate seconds
  lngoutput_time /= 100;
  sprintf(strTemp, "%02d", int(lngoutput_time % 60));
  strtime_str = string(":")+ strTemp + strtime_str;


  // Calculate minutes
  lngoutput_time /= 60;

  sprintf(strTemp, "%02d", int(lngoutput_time));
  strtime_str = string(":") + strTemp + strtime_str;

  // Calculate hours
  lngoutput_time /= 60;
  sprintf(strTemp, "%02d", int(lngoutput_time));
  strtime_str = strTemp + strtime_str;

  // Now we have the full time string. Return it!
  return strtime_str;
}


string xmms_controller::get_song_title() {
  int intplaylist_pos = xmms_remote_get_playlist_pos(intsession);
  int intplaylist_len = xmms_remote_get_playlist_length(intsession);
  // Check if the playlist pos is valid (must be >= 0 and < PL_length)
  if (intplaylist_pos >= 0 && intplaylist_pos < intplaylist_len)
    return xmms_remote_get_playlist_title(intsession, xmms_remote_get_playlist_pos(intsession));
  else
    return "";  // An empty returned song name means no song is currently playing
}

string xmms_controller::get_song_file_path() {
  int intplaylist_pos = xmms_remote_get_playlist_pos(intsession);
  int intplaylist_len = xmms_remote_get_playlist_length(intsession);
  // Check if the playlist pos is valid (must be >= 0 and < PL_length)
  if (intplaylist_pos >= 0 && intplaylist_pos < intplaylist_len)
    return xmms_remote_get_playlist_file(intsession, xmms_remote_get_playlist_pos(intsession));
  else
    return "";  // An empty returned song name means no song is currently playing
}


string xmms_controller::get_playlist_file(const int intpos) {
  // Fetch the filename of an arbitrary playlist entry
  return xmms_remote_get_playlist_file(intsession, intpos);
}

void xmms_controller::playlist_delete(const int intpos) {
  // Remove an entry from the playlist
  xmms_remote_playlist_delete(intsession, intpos);
}

int xmms_controller::getvol() {
  // Get the xmms volume (return a value from 0-100%)
  return xmms_remote_get_main_volume(intsession);
}

void xmms_controller::hide_windows() {
  // Hide all the displayed xmms-shell windows
  xmms_remote_main_win_toggle(intsession, false);
  xmms_remote_pl_win_toggle(intsession, false);
  xmms_remote_eq_win_toggle(intsession, false);
}

void xmms_controller::load(bool blnRestartXMMS){
  // Init the mp3 tag object - used for quick mp3 tag detail retrieval

  // Check if XMMS is installed
  if (!file_exists("/usr/bin/xmms")) {
    my_throw("XMMS is not installed!");
  }

  // Kill any open xmms instances also (only if specified), and
  // wait for XMMS to startup

  if (blnRestartXMMS) {
    system("killall -9 xmms &> /dev/null");

    // Now wait for up to 30 seconds for XMMS to start up again.

    // - Hopefully the persistance script is active, and X is running
    log_message("XMMS will be restarted, waiting for it to become active...");
    datetime dtmTimeOut = now() + 30; // Wait for 30 seconds
    do {
      sleep(1);
    } while (now() <= dtmTimeOut && !running());

    if (now() >= dtmTimeOut) {
      // XMMS is still not running after 30 seconds!
      my_throw("XMMS did not restart! Please check the persistence script and the X server");
    } else {
      // XMMS is now running again.
      // - Set the volume of XMMS to something low. The calling program
      // should adjust it correctly. This change is in case the volume was very high
      // when XMMS last saved it's default volume.
//        setvol(20);         - don't  hardcode a volume adjust the volume when XMMS is restarted...  the
                                  // calling program should adjust it to the correct level if necessary.
      log_message("XMMS was restarted and is now running.");
    }
  }


  // Set shuffle off and repeat off, and hide xmms also
  setshuffle(false);
  setrepeat(false);
  hide_windows();
}

void xmms_controller::play(){
  // If xmms is already playing, issuing another play instruction should NOT restart the same song.
  if (!playing()) {
    // Added in player v6.15 - only allow the play to happen if the playlist is not empty
    // (So the player cannot cause XMMS to bring up a "please specify music to play" box)
    int intPlaylistLen = get_playlist_length();
    if (intPlaylistLen > 0) {
      // XMMS's playlist is not empty.
      xmms_remote_play(intsession);
    }
    else {
      // XMMS's playlist is empty! Do not start playing
      my_throw("XMMS playlist is empty, cannot tell XMMS to play!");
    }
  }
}

bool xmms_controller::playing() {
  // XMMS can think it is playing, but it is not playing!
  // - If XMMS-SHELL reports that it is playing, then check the song position continuously
  // (using API calls) for up to 5 seconds (wait for the song position to change or the "playing"
  // status to stop (eg, stopped because changing to a different MP3, etc)

  // Also, interestingly enough, if xmms was playing but then it was paused, the "is_playing" API function
  // returns true. Take this into account (we define playing as actually playing and progressing through the song)
  if (xmms_remote_is_playing(intsession) && !xmms_remote_is_paused(intsession)) {
    // XMMS reports that it is playing. Now run a check, see if the song position is changing.
    long lngStartMusicPos = xmms_remote_get_output_time(intsession); // Song position in milliseconds
    long lngCurrentMusicPos = 0; // retrieve music position during the loop

    datetime dtmTimeOut = now() + 5;        // Now + 5 seconds
    do {
      usleep(100000); // Usleep is a microseconds function - wait 1/10th of a second
      lngCurrentMusicPos = xmms_remote_get_output_time(intsession);
    } while ((now() <= dtmTimeOut) &&                                   // Check for timeout (5 seconds)
                (xmms_remote_is_playing(intsession)) &&                          // Check if XMMS is still playing (can stop during loop)
                (lngStartMusicPos==lngCurrentMusicPos) &&       // Check if the song position has changed
                (lngCurrentMusicPos >= 0));                                  // Final check - returned values must be valid

    // Check if the song position remained static for over 5 seconds
    if (now() >= dtmTimeOut) {
      // We have detected that XMMS reports that it is playing, but actually it is frozen
      // - The most likely cause is a sound service crash.

      // Terminate any open xmms-shell and xmms sessions
      system("killall -9 xmms-shell >& /dev/null");
      system("killall -9 xmms >& /dev/null");
      log_error("XMMS reported it was playing but was actually frozen. XMMS will be restarted. Please check the sound daemon.");

      // Sound playback was actually frozen, not playing, so return false

      return false;

    }
    else return xmms_remote_is_playing(intsession); // No time-out, so return the current "playing" status
  }
  else return false;   // XMMS reports that is is not playing a song (eg: an announcement ended)
}

void xmms_controller::playlist_add_url(const string strURL) {
  // Allocate temporary storage
  gchar * URL  = (gchar *) alloca (strURL.length() + 1); //alloca - storage returned at function exit

  // Copy the playlist location to this new storage location
  strcpy(URL, strURL.c_str());

  xmms_remote_playlist_add_url_string(intsession, URL);
}

void xmms_controller::playlist_clear() {
  xmms_remote_playlist_clear(intsession);
}

void xmms_controller::playlist_clear_all_except_current() {
  // Get the playlist position and the playlist length
  int intPL_pos = xmms_remote_get_playlist_pos(intsession);
  int intPL_len = xmms_remote_get_playlist_length(intsession);

  // Only run the playlist update if the playlist is not already empty
  if (intPL_len > 0 && intPL_pos >= 0) {
    // Calculate :
    //    - 1) Playlist entries to delete before the current entry
    int intdel_before = intPL_pos;
    //    - 2) Playlist entries to delete after the current entry
    int intdel_after = intPL_len - intPL_pos - 1;

    // Now perform the deletions
    for (int i=0;i<intdel_before;i++)
      xmms_remote_playlist_delete(intsession, 0);  // Deletions *before* the current entry

    for (int i=0;i<intdel_after;i++)
      xmms_remote_playlist_delete(intsession, 1);  // Deletions *after* the current entry
  }
}

void xmms_controller::playlist_load(const string strmusic, bool overwrite_default_playlist) {
  // Tell xmms to load a playlist file.
  GList * xmms_list = 0; // Alocate the data structure to send to the xmms API
                                     // I guess this means that you can tell XMMS to load multiple playlist files at once.

  // Create a temp (writable) var to store a playlist entry in
  gchar * playlist  = (gchar *) alloca (strmusic.length() + 1); //alloca - storage returned at function axit
  // Copy the playlist location to this new storage location
  strcpy(playlist, strmusic.c_str());

  // Append the new item to the playlist list
  xmms_list = g_list_append(xmms_list, playlist);

  // Tell XMMS to load the playlist
  xmms_remote_playlist_add(intsession, xmms_list);

  // Overwrite the default xmms playlist if specified
  if (overwrite_default_playlist) {
    cp(strmusic, "~/.xmms/xmms.m3u"); // Throws an exception if the copy fails.
  }

  // Now free the structure;
  g_list_free(xmms_list); // De-allocate the data structure containing a pointer for the xmms API
}

void xmms_controller::playlist_save(const string strpath) {
  // Save the contents of the playlist to an external file.

  // Open the file for writing.
  ofstream PlaylistFile(strpath.c_str());

  // Process the playlist
  long lngplaylist_len = xmms_remote_get_playlist_length(intsession);

  for (int i=0;i<lngplaylist_len;i++) {
    PlaylistFile << xmms_remote_get_playlist_file(intsession, i) << endl;
  }

  // Close the file.
  PlaylistFile.close();
}

void xmms_controller::pause() {
  xmms_remote_pause(intsession);
}

bool xmms_controller::paused() {
  // Is XMMS in a paused state?
  return xmms_remote_is_paused(intsession);
}

bool xmms_controller::running() {
  // Is the XMMS process running?
  return xmms_remote_is_running(intsession);
}

bool xmms_controller::getrepeat() {
  return xmms_remote_is_repeat(intsession);
}

void xmms_controller::setrepeat(bool blnRepeat) {
  if (xmms_remote_is_repeat(intsession) != blnRepeat) {
    xmms_remote_toggle_repeat(intsession);
  }
}

bool xmms_controller::getshuffle() {
  testing_throw;
  return xmms_remote_is_shuffle(intsession);
}

void xmms_controller::setshuffle(bool blnShuffle) {
  if (xmms_remote_is_shuffle(intsession) != blnShuffle) {    
    xmms_remote_toggle_shuffle(intsession);
  }
}

void xmms_controller::setvol(const int intnew_vol, const bool blnverbose) {
  // Volumes used are now on a scale of 0-100
  // - Clip to valid percentages
  int intvol = intnew_vol;
  if (intvol>100) intvol = 100;
  if (intvol<0) intvol = 0;

  xmms_remote_set_main_volume(intsession, intvol);

  // Sometimes XMMS takes a very short period of time (approx 1/5th of a second)
  // after you set the volume, until it reports the changed volume. This may be because of
  // XMMS's ALSA output driver's software volume mixing.
  // So here we wait for it to catch up...
  int intreported_vol = -1;
  do {
    intreported_vol = xmms_remote_get_main_volume(intsession);
    if (intreported_vol != intvol) usleep(1000000/10); // Sleep 1/10th of a second, then check again.
  } while (intreported_vol != intvol);

  // And log the volume change if we are in verbose mode:
  if (blnverbose) log_message("XMMS (session " + itostr(intsession) + ") volume set to " + itostr(intvol) + "%");
}

void xmms_controller::stop() {
  // Stop XMMS playback
  xmms_remote_stop(intsession);
}

bool xmms_controller::stopped() {
  return !xmms_remote_is_playing(intsession);
}

// Fetch & set the current session number:
int xmms_controller::get_session() {
  return intsession;
}

void xmms_controller::set_session(const int intsession_arg) {
  intsession = intsession_arg;
}

#endif // END: #ifdef __linux__
