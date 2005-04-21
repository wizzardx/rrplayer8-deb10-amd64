#include "player_run_data.h"
#include "common/logging.h"
#include "common/my_string.h"
#include "common/linein.h"
#include "common/testing.h"

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
    xmms[intsession].set_session(intsession);

    // Check if the session is running:
    try {
      xmms[intsession].running(); // Throws an exception if xmms is not running.
      xmms[intsession].stop(); // Stop the XMMS session.
      xmms[intsession].setrepeat(false); // Turn off repeat
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
  
  blnlog_all_music_to_db = false; // Set to true when the player wants to log all available music (and the current XMMS playlist) to the database.
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
    if (xmms_usage[intsession] == SU_UNUSED) my_throw("XMMS session was already free, why free it again?");
    // So mark it as unused.
    xmms_usage[intsession] = SU_UNUSED;
  }
  else {
    // We're marking an XMMS session as used.
    // Is the session marked as used by something else?
    if (xmms_usage[intsession] != SU_UNUSED) my_throw("Cannot allocate an XMMS session, it is already allocated!");
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
    // Couldn't find an XMMS session. So there aren't any LineIn sessions allocated.
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
        testing_throw;
        // Does the next item use LineIn?
        if (next_item.strmedia != "LineIn") {
          testing_throw;
          // No. Set the volume to 0.
          linein_setvol(0);
        }
        // Don't do any further LineIn usage manipulation here. It was already changed, by earlier logic.
        testing_throw;
      }
      else {
        // Current item used XMMS. So Stop it's XMMS (it probably already is, but do it anyway).
        int intsession = get_xmms_used(SU_CURRENT_FG);
        xmms[intsession].stop();
        // Free it also:
        set_xmms_usage(intsession, SU_UNUSED);
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
      case SU_CURRENT_FG: my_throw("Logic Error!"); break; //An error. This should have been set to UNUSED earlier!
      case SU_CURRENT_BG: my_throw("Logic Error!"); break; //An error. This should have been set to UNUSED earlier!
      case SU_NEXT_FG: xmms_usage[intsession] = SU_CURRENT_FG; break; // Next FG becomes current FG.
      case SU_NEXT_BG: xmms_usage[intsession] = SU_CURRENT_BG; break; // Next BG becomes current FG.
      default: my_throw("Logic error!"); // Unknown XMMS usage for the session!
    }
  }

  // Now change next_item over to the current_item:
  current_item = next_item;

  // Next Item is now unloaded:
  next_item.reset();
}
