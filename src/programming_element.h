
#ifndef PROGRAMMING_ELEMENT_H
#define PROGRAMMING_ELEMENT_H

#include <deque>
#include <string>
#include "categories.h"

using namespace std;

// Represents a radio "programming element" to be played back by the Player.
// A programming element is a song, commercial, talk time, etc.
// Basically something to be played back. (Not just MP3, OGG, etc, it can also mean LineIn)

class programming_element {
public: 
  programming_element();  // Constructor
  ~programming_element(); // Destructor

  void reset(); // Reset data in this object

  // Attributes:
  bool blnloaded;    // Has data been loaded into this object?
  seg_category cat;  // Category of this element (Music, Sweeper, Promo, etc)
  string strmedia;   // Media to play back. Special code "LineIn" means play through the LineIn
  string strvol;     // Vol % (of total announce volume).
                     //  - "PROMO" means use announcement vol.
                     //  - "MUSIC" means use music volume

  bool blnmusic_bed; // Set to true if this item has a music bed (underlying music)
  struct music_bed { // Populated if blnmusic_bed is true.
    string strmedia;  // What to play as underlying music. "" = no if there is no
    string strvol;    // What volume to play it at
    int intstart_ms;  // What time during the item the music bed is started. Default: 0 (beginning).
    int intlength_ms;    // How long the underlying music will play for before being stopped. Default: -1 (end when the item ends, or when the underlying music ends. No repeat)
                         // If the length is left at INT_MAX, then we don't know the length (eg: silence or linein).
                         
    

    // For music bed events in an item that were already handled (ie, during the crossfade from the previous item to this item.
    struct already_handled {
      bool blnstart;
      bool blnstop;
    } already_handled;
  } music_bed;

  // Additional info if this is a promo:
  struct promo {
    long lngtz_slot; // When we're done playing the item we update the database.
  } promo;
};

// A list of programming elements (eg: an announcement batch):
typedef deque <programming_element> programming_element_list;

#endif
