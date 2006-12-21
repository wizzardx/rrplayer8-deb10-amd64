
#include "programming_element.h"
#include "common/testing.h"

// Constructor
programming_element::programming_element() {
  reset(); // Reset data in this object
}

// Destructor
programming_element::~programming_element() {
}

// Reset data in this object
void programming_element::reset() {
  blnloaded = false; // Data has not been loaded into this object.
  cat      = SCAT_SILENCE;
  strmedia = "";
  strvol   = "";
  blnmusic_bed = false;
  music_bed.strmedia    = "";
  music_bed.strvol      = "";
  dtmstarted = datetime_error;
  music_bed.intstart_ms = -1;
  music_bed.intlength_ms = -1;
  music_bed.already_handled.blnstart = false;
  music_bed.already_handled.blnstop = false;
  promo.lngtz_slot = -1;
}

// A cache of programming element lists, used when we are in a hurry to get a playlist:
// A class used by generate_playlist to remember recent programming element lists.
cpel_cache pel_cache; // Global variable for player storage of recent programming element lists

void cpel_cache::clear() {
  cache.clear();
}

bool cpel_cache::has(const string & id) {
  tidy();
  cache_type::iterator i = cache.begin();
  return i != cache.end();
}

bool cpel_cache::get(const string & id, programming_element_list & pel) {
  tidy();
  // Check if there is a cached record for the source:
  {
    cache_type::const_iterator i = cache.find(id);
    if (i == cache.end()) {
      // Could not find it.
      return false;
    }
    else {
      // Found it:
      pel = i->second.pel;
      return true;
    }
  }
}

void cpel_cache::set(const string & id, const programming_element_list & pel) {
  pel_info pi;
  pi.pel = pel;
  pi.cached_time = now();
  cache[id] = pi;
}

void cpel_cache::tidy() {
  // Tidy up old records (older than 6 hours):
  cache_type::iterator i = cache.begin();
  while (i != cache.end()) {
    // Store the current iterator value and go to the next
    // (iterators are invalidated when the current record is deleted)
    cache_type::iterator i_old = i++;
    // Old record?
    if (i_old->second.cached_time < now() - (6*60*60)) {
      // Yes. Delete it:
      cache.erase(i_old);
    }
  }
}
