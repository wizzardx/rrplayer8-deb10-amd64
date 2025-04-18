
#ifndef PLAYER_RUN_DATA_H
#define PLAYER_RUN_DATA_H

#include "programming_element.h"
#include "segment.h"
#include "player_constants.h"
#include "player_types.h"
#include "common/xmms_controller.h"

class player_run_data {
public:

  // Constructor
  player_run_data();

  // Run this to clear out members to sane defaults
  void clear();

  // Run this to reset/re-initialize playback status.
  void reset_playback();

  /// Programming elements (current item, next item):
  programming_element current_item; ///< Current programming element being played. Includes special events, etc.
  programming_element next_item;    ///< Next programming element to be played.

  // The current Format Clock segment:
  ap_segment current_segment;

  sound_usage xmms_usage[intmax_xmms]; /// What the xmms sessions are being used for
  sound_usage linein_usage; /// What LineIn is being used for.

  /// Fetch an unused xmms session:
  int get_free_xmms_session();

  /// Set the status of an XMMS session:
  void set_xmms_usage(const int intsession, const sound_usage sound_usage);

  /// Fetch which XMMS session is being used by a given "usage". eg, item fg, item bg, etc.
  /// Throws an exception if we can't find which XMMS session is being used. Also if LineIn is being used.
  int get_xmms_used(const sound_usage sound_usage);

  /// Returns true if linein is being used for the specified usage (foreground, underlying music, etc).
  bool uses_linein(const sound_usage sound_usage);

  /// Returns true if there is already a "sound resource" allocated for the specified usage.
  bool sound_usage_allocated(const sound_usage sound_usage);

  /// Use this to transition info, resource allocation, etc, etc from "next" to "current".
  void next_becomes_current();

  // How long Format Clock Segments are currently "delayed" by. Segments are "delayed" (ie, they start & end later)
  // when earlier segments take too long to play. This figure will go up between non-music segments, up to the
  // limit (6 mins), and then is reduced when we hit a music segment. Music segments are reduced down to a minimum
  // of 30 seconds to reclaim time back from this "push back" factor. This delay is reset once an hour to 0 (possibly causing
  // segments to be lost). This figure basically causes us to query the database for segments in the past.
  int intsegment_delay;

  programming_element_list waiting_promos; ///< List of promos waiting to play. Populated by get_next_item_promo

  datetime dtmlast_promo_batch_item_played; ///< When the last promo batch item played. Used for determining when we can fetch more promo batches

  /// Set to true when the player wants to log 1) the XMMS music playlist, and 2) All available music on the machine.
  bool blnlog_all_music_to_db;

  /// Set to true when the player wants to reload the current segment (eg, a RPLS command was processed)
  bool blnforce_segment_reload;
};

#endif

