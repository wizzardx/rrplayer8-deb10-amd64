/// @file
/// An xmms api wrapper class.
/// To use this library:
/// - Install the libglib1.2-dev package.
/// - include the following CPPFLAGS:   /usr/include/glib-1.2/ -I /usr/lib/glib/include/
/// - Link against: /usr/lib/libglib.so

#ifndef XMMS_CONTROLLER_H
#define XMMS_CONTROLLER_H

#include <string>
#include <vector>
#include <glib.h>
#include "mp3_tags.h"

namespace xmms_controller {
  class xmms_controller {
  public:
    xmms_controller(); ///< sesssion defaults to 0. Use set_session to change it.
    xmms_controller(const int intsession);
    ~xmms_controller();

    int get_playlist_length();
    int get_playlist_pos();

    int get_song_length(); ///< Returns the current song length in seconds
    int get_song_length_ms(); ///< Returns the current song length in milliseconds

    int get_song_pos(); ///< Returns the current song pos in seconds
    int get_song_pos_ms(); ///< Returns the current song pos in milliseconds
    void set_song_pos_ms(const int intpos); ///< Set the current song pos in milliseconds

    string get_song_time_str(); ///< Return something like: 20:00.00
    string get_song_title(); ///< Returns the current strong name
    string get_song_file_path();  ///< Returns the path and filename of the current songj

    string get_playlist_file(const int intpos); ///< Fetch the filename of an arbitrary playlist entry
    void playlist_delete(const int intpos); ///< Remove an entry from the playlist

    int getvol(); ///< Retrieve XMMS's curent volume
    void hide_windows(); ///< Hide all the displayed xmms-shell windows
    void play();
    bool playing();
    void playlist_add_url(const string strURL);
    void playlist_clear();
    void playlist_clear_all_except_current(); ///< Useful to avoid interrupting the current song.
    void playlist_load(const string strmusic, bool overwrite_default_playlist = false);
    void playlist_save(const string strpath);
    void pause();
    bool paused();
    void stop();
    bool stopped();

    bool getrepeat();
    void setrepeat(bool blnRepeat);
    bool getshuffle();
    void setshuffle(bool blnShuffle);
    void setvol(const int intnew_vol);

    // Equaliser pre-amp:
    gfloat get_eq_preamp();
    void set_eq_preamp(const gfloat preamp);

    // Fetch & set the current session number:
    int get_session();
    void set_session(const int intsession);

    // XMMS process-management:
    bool running(); ///< Is the XMMS process running?
    void set_pid(const int pid); ///< Set the PID
    void kill();    ///< Kill the session (if the PID is known)
  private:
    int intsession; ///< Which session this controller will talk to.
    int intpid;     ///< PID of the XMMS session
  };

  // Management of multiple XMMS sessions:

  /// Client code can refer to individual XMMS sessions via this vector
  extern vector <xmms_controller> xmms;

  /// Set the number of XMMS sessions used
  void set_num_xmms_sessions(const int max);

  /// Start extra XMMS sessions if too few are running, and kill excess sessions
  /// if too many are running.
  void ensure_correct_num_xmms_sessions_running();
}

#endif
