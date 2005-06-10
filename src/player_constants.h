#ifndef PLAYER_CONSTANTS_H
#define PLAYER_CONSTANTS_H

// Some player-related constants:
const string PLAYER_DIR      = "/data/radio_retail/progs/player/"; ///< Player program directory. Binary & logfile lives here.
const string PLAYER_LOG_FILE = PLAYER_DIR + "player.log";

const int intmax_xmms = 4;                           ///< Number of XMMS sessions required. Multiple XMMS sessions are used for playing music beds and performing custom crossfades.
                                                     ///< crossfading between two items, both with underlying music.
const int intmax_segment_push_back = 2*60*60;        ///< Maximum amount of time in seconds that segments will be
                                                     ///< "pushed back" because previous segments played for too long.
const bool blndebug = false;                         ///< Is the player in debugging mode? The player will log extra output.

#endif
