#ifndef PLAYER_TYPES_H
#define PLAYER_TYPES_H

/// Track what a given sound resource (linein, xmms session) is being used for:
enum sound_usage {
  SU_UNUSED,     ///< Not used by an item.
  SU_CURRENT_FG, ///< Sound resource is busy playing the current item.
  SU_CURRENT_BG, ///< Sound resource is busy playing the current item's underlying music.
  SU_NEXT_FG,    ///< Sound resource is busy playing the next item
  SU_NEXT_BG     ///< Sound resource is busy playing the next item's underlying music
};

#endif
