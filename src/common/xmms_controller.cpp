
#include "xmms_controller.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h> // sleep() functon
// #include "xmms/xmmsctrl.h" // XMMS API functions

#include "fake_xmmsctrl.h"

#include "glib.h" // Needed to access some "xmmsctrl" functions
#include "string_splitter.h"
#include "exception.h"
#include "file.h"
#include "my_string.h"
#include "system.h"
#include "math.h"
#include "testing.h"

namespace xmms_controller {
  xmms_controller::xmms_controller() {
    intsession = 0;
    intpid = -1; // Unknown until start_process() is called
  }

  xmms_controller::xmms_controller(const int intsession_arg){
    intsession = intsession_arg;
  }

  xmms_controller::~xmms_controller(){
  }

  int xmms_controller::get_playlist_length(){
    return fake_xmms_remote_get_playlist_length(intsession);
  }

  int xmms_controller::get_song_length() {
    // Returns the current song length in seconds
    return get_song_length_ms() / 1000;
  }

  int xmms_controller::get_song_length_ms() {
    return fake_xmms_remote_get_current_song_length_ms(intsession);
/*

    undefined_throw;
    return 5000;*/
//     // Returns the current song length in milliseconds
//     int intplaylist_pos = fake_xmms_remote_get_playlist_pos(intsession);
//     int intplaylist_len = fake_xmms_remote_get_playlist_length(intsession);
//     // Check if the playlist pos is valid (must be >= 0 and < PL_length)
//     if (intplaylist_pos >= 0 && intplaylist_pos < intplaylist_len) {
//       // xmms returns the song length to us in milliseconds, convert to seconds.
//       return  fake_xmms_remote_get_playlist_time(intsession, fake_xmms_remote_get_playlist_pos(intsession));
//     }
//     else {
//       my_throw("Bad playlist position!");
//     }
  }

  int xmms_controller::get_playlist_pos() {
    undefined_throw;
    return 0;
//     // Return the current playlist position (0-based)
//     return fake_xmms_remote_get_playlist_pos(intsession);
  }

  int xmms_controller::get_song_pos() {
    // Value returned is in milleseconds. Convert to seconds.
    return fake_xmms_remote_get_output_time(intsession)/1000;
  }

  int xmms_controller::get_song_pos_ms() {
    return fake_xmms_remote_get_output_time(intsession);
  }

  void xmms_controller::set_song_pos_ms(const int intpos) {
    undefined_throw;
//     // Set song position in milliseconds
//     fake_xmms_remote_jump_to_time(intsession, intpos);
//     // Don't check the new song position, because it takes a short time for
//     // XMMS to actually jump to the requested position
  }

  string xmms_controller::get_song_time_str() {
    undefined_throw;
    return "00:12:30";

//     // Retrieve the Current song position and make an attempt to format it.
//     // The expected format is "00:00.00"
//     long lngoutput_time = fake_xmms_remote_get_output_time(intsession) / 10; // Returns 1000'ths, we start with 100'ths
//     string strtime_str = "";
//
//     // Calculate hundredths of seconds
//     char strTemp[10];
//     sprintf(strTemp, "%02d", int(lngoutput_time % 100));
//     strtime_str = string(".") + strTemp;
//
//     // Calculate seconds
//     lngoutput_time /= 100;
//     sprintf(strTemp, "%02d", int(lngoutput_time % 60));
//     strtime_str = string(":")+ strTemp + strtime_str;
//
//     // Calculate minutes
//     lngoutput_time /= 60;
//     sprintf(strTemp, "%02d", int(lngoutput_time));
//     strtime_str = string(":") + strTemp + strtime_str;
//
//     // Calculate hours
//     lngoutput_time /= 60;
//     sprintf(strTemp, "%02d", int(lngoutput_time));
//     strtime_str = strTemp + strtime_str;
//
//     // Now we have the full time string. Return it!
//     return strtime_str;
  }

  string xmms_controller::get_song_title() {
    return fake_xmms_remote_get_current_song_title(intsession);

//     int intplaylist_pos = fake_xmms_remote_get_playlist_pos(intsession);
//     int intplaylist_len = fake_xmms_remote_get_playlist_length(intsession);
//     // Check if the playlist pos is valid (must be >= 0 and < PL_length)
//     if (intplaylist_pos >= 0 && intplaylist_pos < intplaylist_len) {
//       gchar * ret = fake_xmms_remote_get_playlist_title(intsession, fake_xmms_remote_get_playlist_pos(intsession));
//       if (ret == NULL) my_throw("fake_xmms_remote_get_playlist_title() (to XMMS session " + itostr(intsession) + ") returned a NULL string pointer!");
//       return ret;
//     }
//     else
//       return "";  // An empty returned song name means no song is currently playing
  }

  string xmms_controller::get_song_file_path() {
    return fake_xmms_remote_get_current_song_path(intsession);
//     int intplaylist_pos = fake_xmms_remote_get_playlist_pos(intsession);
//     int intplaylist_len = fake_xmms_remote_get_playlist_length(intsession);
//     // Check if the playlist pos is valid (must be >= 0 and < PL_length)
//     if (intplaylist_pos >= 0 && intplaylist_pos < intplaylist_len) {
//       gchar * ret = fake_xmms_remote_get_playlist_file(intsession, fake_xmms_remote_get_playlist_pos(intsession));
//       if (ret == NULL) my_throw("fake_xmms_remote_get_playlist_file() (to XMMS session " + itostr(intsession) + ") returned a NULL string pointer!");
//       return ret;
//     }
//     else
//       return "";  // An empty returned song name means no song is currently playing
  }

  string xmms_controller::get_playlist_file(const int intpos) {
    undefined_throw;
    return "/fake/path/to/playlist_file.mp3";
//     // Fetch the filename of an arbitrary playlist entry
//     gchar * ret = fake_xmms_remote_get_playlist_file(intsession, intpos);
//     if (ret == NULL) my_throw("fake_xmms_remote_get_playlist_file() (to XMMS session " + itostr(intsession) + ") returned a NULL string pointer!");
//     return ret;
  }

  void xmms_controller::playlist_delete(const int intpos) {
    undefined_throw;
//     // Remove an entry from the playlist
//     fake_xmms_remote_playlist_delete(intsession, intpos);
  }

  int xmms_controller::getvol() {
    // Get the xmms volume (return a value from 0-100%)
    return fake_xmms_remote_get_main_volume(intsession);
  }

  void xmms_controller::hide_windows() {
    // We've switched over to MPD, so there are no GUI windows that need to be hidden
//     // Hide all the displayed xmms-shell windows
//     fake_xmms_remote_main_win_toggle(intsession, false);
//     fake_xmms_remote_pl_win_toggle(intsession, false);
//     fake_xmms_remote_eq_win_toggle(intsession, false);
  }

  void xmms_controller::play(){
    // If xmms is already playing, issuing another play instruction should NOT restart the same song.
    if (!playing()) {
      // Added in player v6.15 - only allow the play to happen if the playlist is not empty
      // (So the player cannot cause XMMS to bring up a "please specify music to play" box)
      int intPlaylistLen = get_playlist_length();
      if (intPlaylistLen > 0) {
        // XMMS's playlist is not empty. Start playback:
        fake_xmms_remote_play(intsession);

        // Wait for up to 0.5s for XMMS to start reporting that it's now
        // playing:
        int check_count = 0;
        const int max_checks = 5;
        bool done = false;
        while (!done) {
          check_count++;
          if (fake_xmms_remote_is_playing(intsession)) {
            done = true;
          }
          else {
            if (check_count >= max_checks) {
              my_throw("XMMS session " + itostr(intsession) +
                       " didn't start playing!");
            }
            else {
              // Sleep for 0.1s before the next check
              usleep(100000);
            }
          }
        }

        // Check that playback (really, not just reported as) started:
        if (!playing()) {
          my_throw("XMMS session " + itostr(intsession) + " didn't start playing!");
        }
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
    if (fake_xmms_remote_is_playing(intsession) && !fake_xmms_remote_is_paused(intsession)) {
      // XMMS reports that it is playing. Now run a check, see if the song position is changing.
      long lngStartMusicPos = fake_xmms_remote_get_output_time(intsession); // Song position in milliseconds
      long lngCurrentMusicPos = 0; // retrieve music position during the loop

      datetime dtmTimeOut = now() + 5;        // Now + 5 seconds
      do {
        usleep(100000); // Usleep is a microseconds function - wait 1/10th of a second
        lngCurrentMusicPos = fake_xmms_remote_get_output_time(intsession);
      } while ((now() <= dtmTimeOut) &&                                   // Check for timeout (5 seconds)
                  (fake_xmms_remote_is_playing(intsession)) &&                          // Check if XMMS is still playing (can stop during loop)
                  (lngStartMusicPos==lngCurrentMusicPos) &&       // Check if the song position has changed
                  (lngCurrentMusicPos >= 0));                                  // Final check - returned values must be valid

      // Check if the song position remained static for over 5 seconds
      if (now() >= dtmTimeOut) {
        // We have detected that XMMS reports that it is playing, but actually it is frozen
        // - The most likely cause is a sound service crash.
        kill();
        my_throw("XMMS session " + itostr(intsession) + " reported that it was playing, put it was actually frozen! Session was killed. Please check the sound daemon");
      }
      else return fake_xmms_remote_is_playing(intsession); // No time-out, so return the current "playing" status
    }
    else return false;   // XMMS reports that is is not playing a song (eg: an announcement ended)
  }

  void xmms_controller::playlist_add_url(const string strURL) {
    // Allocate temporary storage
    gchar * URL  = (gchar *) alloca (strURL.length() + 1); //alloca - storage returned at function exit

    // Copy the playlist location to this new storage location
    strcpy(URL, strURL.c_str());

    fake_xmms_remote_playlist_add_url_string(intsession, URL);
  }

  void xmms_controller::playlist_clear() {
    fake_xmms_remote_playlist_clear(intsession);
  }

  void xmms_controller::playlist_clear_all_except_current() {
    undefined_throw;
//     // Get the playlist position and the playlist length
//     int intPL_pos = fake_xmms_remote_get_playlist_pos(intsession);
//     int intPL_len = fake_xmms_remote_get_playlist_length(intsession);
//
//     // Only run the playlist update if the playlist is not already empty
//     if (intPL_len > 0 && intPL_pos >= 0) {
//       // Calculate :
//       //    - 1) Playlist entries to delete before the current entry
//       int intdel_before = intPL_pos;
//       //    - 2) Playlist entries to delete after the current entry
//       int intdel_after = intPL_len - intPL_pos - 1;
//
//       // Now perform the deletions
//       for (int i=0;i<intdel_before;i++)
//         fake_xmms_remote_playlist_delete(intsession, 0);  // Deletions *before* the current entry
//
//       for (int i=0;i<intdel_after;i++)
//         fake_xmms_remote_playlist_delete(intsession, 1);  // Deletions *after* the current entry
//     }
  }

  void xmms_controller::playlist_load(const string strmusic, bool overwrite_default_playlist) {
    undefined_throw;
/*
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
    fake_xmms_remote_playlist_add(intsession, xmms_list);

    // Overwrite the default xmms playlist if specified
    if (overwrite_default_playlist) {
      cp(strmusic, "~/.xmms/xmms.m3u"); // Throws an exception if the copy fails.
    }

    // Now free the structure;
    g_list_free(xmms_list); // De-allocate the data structure containing a pointer for the xmms API*/
  }

  void xmms_controller::playlist_save(const string strpath) {
    undefined_throw;
/*
    // Save the contents of the playlist to an external file.

    // Open the file for writing.
    ofstream PlaylistFile(strpath.c_str());

    // Process the playlist
    long lngplaylist_len = fake_xmms_remote_get_playlist_length(intsession);

    for (int i=0;i<lngplaylist_len;i++) {
      PlaylistFile << fake_xmms_remote_get_playlist_file(intsession, i) << endl;
    }

    // Close the file.
    PlaylistFile.close();*/
  }

  void xmms_controller::pause() {
    undefined_throw;
//     fake_xmms_remote_pause(intsession);
  }

  bool xmms_controller::paused() {
    undefined_throw;
    return false;
//     // Is XMMS in a paused state?
//     return fake_xmms_remote_is_paused(intsession);
  }

  bool xmms_controller::getrepeat() {
    return fake_xmms_remote_is_repeat(intsession);
  }

  void xmms_controller::setrepeat(bool blnRepeat) {
    if (fake_xmms_remote_is_repeat(intsession) != blnRepeat) {
      fake_xmms_remote_toggle_repeat(intsession);
    }
  }

  bool xmms_controller::getshuffle() {
    undefined_throw;
    return false;
//     return fake_xmms_remote_is_shuffle(intsession);
  }

  void xmms_controller::setshuffle(bool blnShuffle) {
    undefined_throw;
//     if (fake_xmms_remote_is_shuffle(intsession) != blnShuffle) {
//       fake_xmms_remote_toggle_shuffle(intsession);
//     }
  }

  void xmms_controller::setvol(const int intnew_vol) {
    // Volumes used are now on a scale of 0-100
    // - Clip to valid percentages
    int intvol = intnew_vol;
    if (intvol>100) intvol = 100;
    if (intvol<0) intvol = 0;

    // Quit if the XMMS volume is already correct:
    int intcurrent_vol = fake_xmms_remote_get_main_volume(intsession);
    if (intcurrent_vol == intvol) return;

    fake_xmms_remote_set_main_volume(intsession, intvol);

    // Sometimes XMMS takes a very short period of time (approx 1/5th of a second)
    // after you set the volume, until it reports the changed volume. This may be because of
    // XMMS's ALSA output driver's software volume mixing.
    // So here we wait for it to catch up...
    int intreported_vol = -1;
    int intattempts_remaining = 50; // Loop up to 50 times (5 seconds), waiting for the volume to change.
    do {
      intreported_vol = fake_xmms_remote_get_main_volume(intsession);
      if (intreported_vol != intvol) usleep(1000000/10); // Sleep 1/10th of a second, then check again.
      --intattempts_remaining;
    } while ((intreported_vol != intvol) && (intattempts_remaining > 0));

    // Did the loop timeout?
    if (intattempts_remaining <= 0) my_throw("Could not set XMMS volume! (Currently: " + itostr(intreported_vol) + "%. Wanted: " + itostr(intvol) + "%)");

    // And log the volume change:
    log_line("XMMS (session " + itostr(intsession) + ") volume set to " + itostr(intvol) + "%");
  }

  gfloat xmms_controller::get_eq_preamp() {
    undefined_throw;
//     // Return the XMMS equaliser pre-amp (from -20db to +20db)
//     return fake_xmms_remote_get_eq_preamp(intsession);
  }

  void xmms_controller::set_eq_preamp(const gfloat preamp) {
    // There is no concept of preamp (or other XMMS-like equalizer settings)
    // in MPD, as far as I know, so we don't do anything over here.

//     gfloat new_preamp = CLAMP(preamp, -19.999, 19.999);
//
//     // Quit if the new value is very close to the current one:
//     gfloat old_preamp = fake_xmms_remote_get_eq_preamp(intsession);
//     if (fabs(old_preamp - new_preamp) <= 1.0) return;
//
//     // Set the new pre-amp volume and log it:
//     fake_xmms_remote_set_eq_preamp(intsession, new_preamp);
//     log_line("XMMS (session " + itostr(intsession) + ") equalizer pre-amp set to " + dtostr(new_preamp) + "db");
  }

  void xmms_controller::stop() {
    // Stop XMMS playback
    fake_xmms_remote_stop(intsession);
  }

  bool xmms_controller::stopped() {
    return !fake_xmms_remote_is_playing(intsession);
  }

  // Fetch & set the current session number:
  int xmms_controller::get_session() {
    return intsession;
  }

  void xmms_controller::set_session(const int intsession_arg) {
    intsession = intsession_arg;
  }

  bool xmms_controller::running() {
    // Is the XMMS process running?
    return fake_xmms_remote_is_running(intsession);
  }

  void xmms_controller::set_pid(const int pid) {
    // Set the PID
    intpid = pid;
  }

  void xmms_controller::kill() {
    undefined_throw;

//     // Kill the XMMS session:
//     if (!running())   my_throw("XMMS session " + itostr(intsession)+ " is not running, cannot kill it!");
//     if (intpid == -1) {
//       // If the PID is not known (eg, XMMS was running before the player started)
//       // then kill all XMMS sessions
//       log_warning("Don't know PID for XMMS session " + itostr(intsession) + ", killing all XMMS sessions instead!");
//       system("killall -9 -q xmms");
//     }
//     else {
//       system(string("kill -9 -q " + itostr(intpid) + " &> /dev/null").c_str());
//     }
  }

  // Management of multiple XMMS sessions:
  vector <xmms_controller> xmms;
  int num_xmms_sessions = -1;

  void set_num_xmms_sessions(const int max) {
    // Set the number of XMMS sessions used
    if (num_xmms_sessions != -1) {
      my_throw("Number of XMMS sessions is already set to " + itostr(num_xmms_sessions) + ", cannot set it again!");
    }
    num_xmms_sessions = max;

    for (int i = 0; i < num_xmms_sessions; i++) {
      xmms.push_back(xmms_controller(i));
    }
  }

  void ensure_correct_num_xmms_sessions_running() {
    // We're now managing (2) MPD sessions externally, so the Player doesn't
    // need to do anything here, unlike with XMMS.

//       testing;
//       cout << "TODO: Implement ensure_correct_num_xmms_sessions_running()" << endl;

/*
    // Start extra XMMS sessions if too few are running, and kill excess sessions
    // if too many are running.

    // Check that set_num_xmms_sessions() has been called:
    if (num_xmms_sessions == -1)
      my_throw("set_num_xmms_sessions() must be called first!");

    // Check if there are any XMMS sessions running after the ones being managed:
    bool killall = false; // Set to true if we need to kill all XMMS sessions.
    for (int i=num_xmms_sessions; i < 16; i++) {
      if (fake_xmms_remote_is_running(i)) {
        log_warning("XMMS session " + itostr(i) + " is running! (No sessions above " + itostr(num_xmms_sessions - 1) + " should be running)");
        killall = true; // We need to kill all the XMMS sessions!
      }
    }

    if (killall) {
      // Unexpected XMMS sessions were running, need to kill them all:
      log_warning("Killing all XMMS sessions...");
      system("killall -9 -q xmms &> /dev/null");
      // Wait a second, give the system some time to kill the sessions:
      sleep(1);
    }

    // Now startup additional sessions if needed:
    for (int i=0; i < num_xmms_sessions; i++) {
      if (!xmms[i].running()) {
        log_warning("XMMS session " + itostr(i) + " is not running, starting it...");
        string stroutput;
        system_capture_out_throw("/usr/bin/rrstart-xmms", stroutput);
        int pid = strtoi(stroutput); // Script returns the PID number

        // Fetch the PIDs of all running XMMS sessions:
        string strxmms_pids;
        system_capture_out("/bin/pidof xmms", strxmms_pids);
        if (strxmms_pids == "") my_throw("I started an XMMS session, but no XMMS processes are running!");

        // An XMMS session was started successfully.
        // Wait for up to 30 seconds for the XMMS session to finish starting up
        // (eg, the process starts, but it takes a while to start responding to
        // the control socket)
        int timeout=30;

        while (pid_exists(pid) && !xmms[i].running() && timeout > 0) {
          log_message("Waiting for new XMMS session running under PID " + itostr(pid) + " to respond...");
          sleep(10);
          timeout -= 10; // Try every 10 seconds, if we try every second there is a good chance of XMMS "freezing" and being killed.
        }

        if (!pid_exists(pid))
          log_warning("New XMMS session terminated unexpectedly!");
        else if (timeout <= 0)
          log_warning("New XMMS session didn't respond!");

        if (!xmms[i].running()) {
          // The XMMS session number that should have been started isn't running. Get more info.
          string strxmms_session_list = "";
          int intsession_count = 0;
          for (int session=0; session < 16; session++) {
            if (fake_xmms_remote_is_running(session)) {
              intsession_count++;
              if (!strxmms_session_list.empty()) strxmms_session_list += " ";
              strxmms_session_list += itostr(session);
            }
          }

          // Check if we found anything:
          if (intsession_count == 0) {
            log_warning("Started an XMMS session, but could not find any sessions! What's going on?");
          }
          else {
            log_warning("Started an XMMS session, but the one we wanted (" + itostr(i) + ") was not found. Found these sessions: " + strxmms_session_list);
          }

          // Now kill all running XMMS sessions
          log_warning("Killing all XMMS processes (" + strxmms_pids + "), hopefully this will help");
          system("killall -9 -q xmms");

          // Now throw an exception so the main logic tries again later:
          my_throw("Problems starting an XMMS session!");
        }

        // We know the PID of the XMMS session:
        xmms[i].set_pid(pid);

        // Now hide all the windows & setup shuffle & repeat:
        xmms[i].hide_windows();
        xmms[i].setshuffle(false);
        xmms[i].setrepeat(false);
      }
    }*/
  }
}
