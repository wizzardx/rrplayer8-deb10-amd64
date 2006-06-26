#ifndef PLAYER_CONSTANTS_H
#define PLAYER_CONSTANTS_H

// Some player-related constants:
const string PLAYER_DIR      = "/data/radio_retail/progs/player/"; ///< Player program directory. Binary & logfile lives here.
const string PLAYER_LOG_FILE = PLAYER_DIR + "player.log";

const int intmax_xmms = 2;                           ///< Number of XMMS sessions required. Only 2 are needed until we start using music beds.
                                                     ///< crossfading between two items, both with underlying music.
const int intmax_segment_push_back = 2*60*60;        ///< Maximum amount of time in seconds that segments will be
                                                     ///< "pushed back" because previous segments played for too long.
const int intprevent_song_repeat_factor = 75;        ///< After a music item plays, a minimumn number of other songs need to play
                                                     ///< before the same song can be played again. The amount of other songs
                                                     ///<that need to play is [intprevent_song_repeat_factor]% of the current music
                                                     ///<playlists length

#endif

