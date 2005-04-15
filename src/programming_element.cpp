
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
  music_bed.intstart_ms = -1;
  music_bed.intlength_ms = -1;
  music_bed.already_handled.blnstart = false;
  music_bed.already_handled.blnstop = false;
  promo.lngtz_slot = -1;
}

