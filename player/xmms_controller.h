/***************************************************************************
                          xmms_controller.h  -  description
                             -------------------
    version              : 0.06
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifndef XMMS_CONTROLLER_H
#define XMMS_CONTROLLER_H
#define XMMS_CONTROLLER_H_VERSION 6 // Meaning 0.06

#include <string>
#include "rr_utils.h"
#include "mp3_tags.h"
#include "check_library_versions.h" // Always last: Check the versions of included libraries.

//// Structure used for saving and restoring the XMMS state.
//struct xmms_state {
//  string strPlaylist;   // Playlist file to restore from/save to.
//  int intVolume;      // Volume at the time of saving
//  long lngPlaylistPos; // Position in the playlist
//  long lngSongPosMS; // Position in the song (milliseconds)
//  bool blnPlaying; // Is XMMS currently in a "playing" state? (can be paused also, but not stopped)
//  bool blnPaused; // is XMMS in a "paused" state?
//};

// With current XMMS settings, sound playback is out of sync with with
//  1) Where XMMS says it is (eg: XMMS says a song just finished),
//  2) XMMS response to when you tell it to do certain things (stop a song, play a song, etc)
//
//  [1] Is the sum of xmms-crossfade + sound driver buffer size.
//  [2] Is just the sound buffer size.

const int XMMS_LATENCY_REPORT=8000; // eg: When XMMS reports that a song is done
const int XMMS_LATENCY_DO=1500;     // eg: When you tell XMMS to play or stop a song.

class xmms_controller {
public:
  xmms_controller();
  ~xmms_controller();

  int get_playlist_length();
  int get_playlist_pos();
  
  int get_song_length(); // Returns the current song length in seconds
  int get_song_pos(); // Returns the current song pos in seconds
  int get_song_pos_ms(); // Returns the current song pos in milliseconds

  string get_song_time_str(); // Return something like: 20:00.00
  string get_song_title(); // Returns the current strong name
  string get_song_file_path();  // Returns the path and filename of the current songj

  string get_playlist_file(const int intpos); // Fetch the filename of an arbitrary playlist entry
  void playlist_delete(const int intpos); // Remove an entry from the playlist
  
  string get_mp3_title(const string strMP3Path); // A bonus function to return a random MP3 tag description    

  double getvol(); // Retrieve XMMS's curent volume
  void hide_windows(); // Hide all the displayed xmms-shell windows
  void load(bool blnRestartXMMS);
  void play();
  bool playing();
  void playlist_add_url(const string strURL);
  void playlist_clear();
  void playlist_clear_all_except_current(); // Useful to avoid interrupting the current song.
  void playlist_load(const string strmusic, bool overwrite_default_playlist = false);
  void playlist_save(const string strpath);
  void pause();
  bool paused();
  bool running();      // Is the XMMS process running?

  void setrepeat(bool blnRepeat);
  void setshuffle(bool blnShuffle);
  void setvol(double NewVol, bool blnVerbose=true);
  void stop();
  
////  // Save and restore status - useful for pausing and resuming
////  bool state_save(xmms_state * State, const string strSavePlaylistTo);
////  bool state_restore(const xmms_state * State);
  
//  void sound_buffer_wait(); // XMMS can be set to pre-buffer playback.
//                                           // This can cause some issues though...
  
  void maintenance_check(); // There are some background tasks that this class likes to run
                                              // (updating changes to an mp3 tag buffer file). This function should be called
                                              // regularly (it is not critical)

// Removed in v6.15 (build 211) - sound latency is now a configurable setting, because the detection
// is unreliable (cannot connect to aRts instore).
//  long get_sound_latency();  // If XMMS has been set to pre-buffer sound, then there will be a lag
//                                             // between XMMS commands and them actually being heard
//                                             // (eg: stop playback). This function checks the XMMS config file to
//                                             // detect this latency. Return result is in milliseconds.

private:
  mp3_tags MP3_Tags; // A collection which is good for buffering and returning tags of MP3s on the machine.

//  bool run_xmmsshell_cmd_file(const string strCmdFileName, const string strPipedShellFileName = "");

//  bool add_waiting_cmd(const string strwaiting_cmd);
//  bool run_cmd_now(const string strcmd, const string stroutput_file="");
//
//  // Buffering of instructions (for speed optimization)
//  int int_buffer_level; // When buffer_level is 0, buffer contents are executed. When
//                                // buffer_commands(true) is called, buffer_level++ and
//                                // buffer_commands(false) -> buffer_level --;
//
//  string strWaitingCommands; // A list of \n separated xmms-shell commands
//
//  // Reuse of status.txt if it was updated recently
//  bool blnreuse_status_txt;
//  DateTime dtm_last_status_txt_generate;
//  int int_status_txt_age_allowed;  // How old the most recently-retrieved status.txt must be before it must be regenerated
//  void generate_status_txt();   // Generate status.txt in the current path if it was not generated recently
};

#endif
