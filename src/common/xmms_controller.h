/***************************************************************************
                          xmms_controller.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifndef XMMS_CONTROLLER_H
#define XMMS_CONTROLLER_H

#include <string>
#include "mp3_tags.h"

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
  xmms_controller(); // sesssion defaults to 0. Use set_session to change it.
  xmms_controller(const int intsession);
  ~xmms_controller();

  int get_playlist_length();
  int get_playlist_pos();
  
  int get_song_length(); // Returns the current song length in seconds
  int get_song_length_ms(); // Returns the current song length in milliseconds  

  int get_song_pos(); // Returns the current song pos in seconds
  int get_song_pos_ms(); // Returns the current song pos in milliseconds

  string get_song_time_str(); // Return something like: 20:00.00
  string get_song_title(); // Returns the current strong name
  string get_song_file_path();  // Returns the path and filename of the current songj

  string get_playlist_file(const int intpos); // Fetch the filename of an arbitrary playlist entry
  void playlist_delete(const int intpos); // Remove an entry from the playlist

  int getvol(); // Retrieve XMMS's curent volume
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
  void stop();
  bool stopped();
  
  bool getrepeat();
  void setrepeat(bool blnRepeat);
  bool getshuffle();
  void setshuffle(bool blnShuffle);
  void setvol(const int intnew_vol, const bool blnverbose=true);

  // Fetch & set the current session number:
  int get_session();
  void set_session(const int intsession);

private:
  int intsession; // Which session this controller will talk to.
};

#endif

