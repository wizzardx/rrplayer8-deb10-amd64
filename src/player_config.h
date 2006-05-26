
#ifndef PLAYER_CONFIG_H
#define PLAYER_CONFIG_H

#include <string>

/// Some settings and configuration read from the config file and from the database.
typedef struct {
  /// Database connection details (player.conf)
  struct db {
    std::string strserver;   ///< Server hostname, IP address, etc
    std::string strdb;       ///< The name of the database
    std::string struser;     ///< The user name
    std::string strpassword; ///< The password
    std::string strport;     ///< The port
  } db;

  // Promo frequency capping options (tbldefs)
  int intmins_to_miss_promos_after; ///< If an announcement was scheduled to play earlier than this amount of time ago, then skip it if it has not already been played.
  int intmax_promos_per_batch;      ///< Limits the number of promos played, even if there is major overschedulig.
  int intmin_mins_between_batches;  ///< Minimum amount of music to play between promo batches

  /// Directories used by the player.
  struct dirs {
    std::string strmp3;
    std::string stradverts;
    std::string strannouncements;
    std::string strspecials;
    std::string strreceived;
    std::string strtoday;
    std::string strprofiles;
  } dirs;

  /// Added in 6.15 (build 330) The location to use for the default music profile:
  std::string strdefault_music_source;

  // Added in 6.21 (build 737) Wait for the current song to end before starting
  // promo playback? Exception: linein music & "force to play now" promos.
  bool blnpromos_wait_for_song_end;

  // New settings added in v7.00
  // Format clock settings:
  bool blnformat_clocks_enabled; ///< Are format clocks used on this system?
  long lngdefault_format_clock;  ///< Database reference to the "default" format clock (to use if
                                 ///< there are problems with the current format clock, or no format clocks
                                 ///< were scheduled.

  // Crossfade settings:
  int intcrossfade_length_ms; ///< Crossfades run for 8000ms. Also music fade-ins and fade-outs.
} player_config;

#endif
