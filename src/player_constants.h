#ifndef PLAYER_CONSTANTS_H
#define PLAYER_CONSTANTS_H

// Some player-related constants:
const string PLAYER_DIR = "/data/radio_retail/progs/player/"; ///< Player program directory. Binary lives here.

const int intcrossfade_length_ms             = 8000; ///< Crossfades run for 8000ms. Also music fade-ins and fade-outs.
const int intnext_playback_safety_margin_ms = 18000; ///< How long before important playback events, the player should be ready and
                                                     ///< not run other logic which could cut into time needed for crossfading, etc.
const int intmax_xmms = 4;                           ///< Number of XMMS sessions required. Multiple XMMS sessions are used for playing music beds and performing custom crossfades.
                                                     ///< crossfading between two items, both with underlying music.
const int intmax_segment_push_back = 6*60;           ///< Maximum amount of time in seconds that segments will be
                                                     ///< "pushed back" because previous segments played for too long.
const bool blndebug = false;                         ///< Is the player in debugging mode? The player will log extra output.

#endif
