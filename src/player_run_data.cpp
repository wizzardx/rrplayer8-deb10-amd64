#include "player_run_data.h"
#include "common/logging.h"
#include "common/my_string.h"
#include "common/linein.h"
#include "common/testing.h"
#include "common/system.h"

namespace xmmsc = xmms_controller;

void player_run_data::init() {
  // Run this function to reset/reinitialize playback status.
  log_message("Resetting playback.");

  // Programming elements (current item, next item):
  current_item.reset();
  next_item.reset();

  // The current Format Clock segment:
  current_segment.reset();

  // Now setup xmms sessions and tracking info:
  for (int intsession = 0; intsession < intmax_xmms; intsession++) {
    // Setup a controller for the session:
    xmmsc::xmms[intsession].set_session(intsession);

    // Check if the session is running. If it is, then do some further init.
    try {
      xmmsc::xmms[intsession].running(); // Throws an exception if xmms is not running.
      xmmsc::xmms[intsession].stop(); // Stop the XMMS session.
      xmmsc::xmms[intsession].setrepeat(false); // Turn off repeat
      xmmsc::xmms[intsession].hide_windows(); // Hide all visible XMMS windows
    }
    catch(...) {
      // We don't throw this as an exception. Init will still succeed. Later the
      // check_playback_status() will be called and it will throw an exception.
      log_error("XMMS session " + itostr(intsession) + " is not running!");
    }

    // Setup tracking, so we know which XMMS are currently being used for what:
    xmms_usage[intsession] = SU_UNUSED; // XMMS session is not currently being used.
  }

  // Also reset the linein volume:
  linein_usage = SU_UNUSED;
  linein_setvol(0);

  intsegment_delay = 0; // Segments are currently delayed by this number of seconds.

  waiting_promos.clear(); // List of promos waiting to play. Populated by get_next_item_promo

  dtmlast_promo_batch_item_played = datetime_error; // Set when an item from a promo batch plays

  blnlog_all_music_to_db = false; // Set to true when the player wants to log all available music (and the current XMMS playlist) to the database.

  blnforce_segment_reload = false; // Set to true when the player wants to reload the current segment (eg, a RPLS command was found)

  // Set the PCM volume to 90% - we use software mixing not hardware!
  string strout;
  system_capture_out_throw("/usr/bin/aumix -w 90", strout);
  log_message("PCM volume set to 90%");
}

int player_run_data::get_free_xmms_session() {
  // Fetch an unused xmms session
  try {
    return get_xmms_used(SU_UNUSED); // Returns the first free XMMS session.
  } catch(...) {
    testing_throw;
    my_throw("Could not find any unused XMMS sessions!");
  }
}

void player_run_data::set_xmms_usage(const int intsession, const sound_usage sound_usage) {
  // Set the status of an XMMS session
  // A valid session number?
  if (intsession < 0 || intsession >= intmax_xmms) my_throw("Invalid XMMS session number!");

  // Are we setting the session as UNUSED, or as USED?
  if (sound_usage == SU_UNUSED) {
    // We're marking an XMMS session as unused.
    // Is the session already unused?
    if (xmms_usage[intsession] == SU_UNUSED) {
      my_throw("XMMS session " + itostr(intsession) + " was already free, why free it again?");
    }
    // Is xmms still playing?
    if (xmmsc::xmms[intsession].playing()) {
      my_throw("XMMS session " + itostr(intsession) + " was still playing when it was marked as unused!");
    }

    // So mark it as unused.
    xmms_usage[intsession] = SU_UNUSED;
  }
  else {
    // We're marking an XMMS session as used.
    // Is the session marked as used by something else?
    if (xmms_usage[intsession] != SU_UNUSED) {
      my_throw("Cannot allocate XMMS session " + itostr(intsession) + ", it is already allocated!");
    }
    // Is this "usage" already allocated for something else?
    if (sound_usage_allocated(sound_usage)) my_throw("There is already a sound allocation for this 'usage', cannot make another allocation!");
    // Everything checks out. So mark the XMMS session as used:
    xmms_usage[intsession] = sound_usage;
  }
}

int player_run_data::get_xmms_used(const sound_usage sound_usage) {
  // Fetch which XMMS session is being used by a given "usage". eg, item fg, item bg, etc.
  // - Throws an exception if we can't find which XMMS session is being used.
  for (int intsession=0; intsession < intmax_xmms; ++intsession) {
    if (xmms_usage[intsession] == sound_usage) return intsession;
  }

  // We got this far so there is no XMMS currently being used this way.
  my_throw("I could not find the XMMS session you wanted!");
}

bool player_run_data::uses_linein(const sound_usage sound_usage) {
  // Returns true if linein is being used for the specified usage (foreground, underlying music, etc).
  return linein_usage == sound_usage;
}

bool player_run_data::sound_usage_allocated(const sound_usage sound_usage) {
  // Returns true if there is already a "sound resource" allocated for the specified usage.

  // Is LineIn allocated?
  if (linein_usage == sound_usage) return true;

  // No. Is there an XMMS session allocated?
  try {
    get_xmms_used(sound_usage);
    // No error. So there is an XMMS session reserved for this usage.
    return true;
  } catch (...) {
    // Couldn't find an XMMS session.
    // ie, this "sound usage" hasn't been set to LineIn or an XMMS session
    return false;
  }
}

void player_run_data::next_becomes_current() {
  // Use this to transition info, resource allocation, etc, etc from "next" to "current".

  // First free up resources used by the current item:

  // Do we have a current item? (we don't if we just started playing items).
  if (current_item.blnloaded) {
    // Is the current item silence?
    if (current_item.cat != SCAT_SILENCE) {
      // No.
      // Does it's foreground use Linein?
      if (current_item.strmedia == "LineIn") {
        // Is the next item also LineIn?
        if (next_item.strmedia == "LineIn") {
          // Next item is also linein:
          linein_usage = SU_UNUSED; // LineIn no longer used for anything
        }
        else {
          // Next item is not linein:
          // Set the volume to 0.
          linein_setvol(0);
          linein_usage = SU_UNUSED; // LineIn no longer used for anything
        }
        // If the next item is LineIn, don't update the linein usage here. It was already setup. See
        // "setup_next" handler in player_playback_transition.cpp
      }
      else {
        // Current item used XMMS. It should have already been stopped by the
        // time this function runs.
        int intsession = -1;
        try {
          intsession = get_xmms_used(SU_CURRENT_FG);
        } catch(...) {}
        if (intsession != -1) {
          my_throw("Current item's XMMS session (" + itostr(intsession) + ") should have been freed earlier!");
        }
      }
    }

    // Did the current item have a music bed
    if (current_item.blnmusic_bed) {
      // Yes. Check if it is still in use:
      int intsession = -1;
      try {
        intsession = get_xmms_used(SU_CURRENT_BG);
        // If it is still used this is bad! We didn't free it earlier.
      } catch(...) { }
      if (intsession != -1) my_throw("Music Bed XMMS session should have been freed earlier!");
    }
  }

  // Change the "usage" over from "next" to "current".
  for (int intsession=0; intsession<intmax_xmms; intsession++) {
    switch(xmms_usage[intsession]) {
      case SU_UNUSED: break; // Do nothing.
      case SU_CURRENT_FG: LOGIC_ERROR; break; //An error. This should have been set to UNUSED earlier!
      case SU_CURRENT_BG: LOGIC_ERROR; break; //An error. This should have been set to UNUSED earlier!
      case SU_NEXT_FG: xmms_usage[intsession] = SU_CURRENT_FG; break; // Next FG becomes current FG.
      case SU_NEXT_BG: xmms_usage[intsession] = SU_CURRENT_BG; break; // Next BG becomes current FG.
      default: LOGIC_ERROR; // Unknown XMMS usage for the session!
    }
  }

  // Also change over LineIn:
  switch (linein_usage) {
    case SU_UNUSED: break; // Do nothing.
    case SU_CURRENT_FG: LOGIC_ERROR; break; //An error. This should have been set to UNUSED earlier!
    case SU_CURRENT_BG: LOGIC_ERROR; break; // Cannot use Linein as a music bed!
    case SU_NEXT_FG: linein_usage = SU_CURRENT_FG; break; // Next FG becomes current FG.
    case SU_NEXT_BG: LOGIC_ERROR; break; // Cannot use Linein as a music bed!
    default: LOGIC_ERROR; // Unknown XMMS usage for the session!
  }

  // Now change next_item over to the current_item:
  current_item = next_item;

  // Next Item is now unloaded:
  next_item.reset();
}

