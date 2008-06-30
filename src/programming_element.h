
#ifndef PROGRAMMING_ELEMENT_H
#define PROGRAMMING_ELEMENT_H

#include <deque>
#include <map>
#include <string>
#include "categories.h"
#include "common/my_time.h"
#include "common/psql.h"

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
  datetime dtmstarted; // What date & time the item playback started

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
    bool blnforced_time; // Was this promo scheduled to play at a specific time?
  } promo;

  // Extra information about the MP3, stored in tblinstore_media, by the
  // rrmedia-maintenance service:
  struct media_info {
    // General information
    bool blnloaded; // Has this info been loaded?
    int intlength_ms; // Length of the item in ms
    bool blndynamically_compressed; // Was the item's range dynamically
                                    // compressed?
                                    // - If not then we can't make reasonable
                                    //   volume-related guesses about the end
                                    //   of the song

    // MP3 end:
    int intend_silence_start_ms; // Where the silence at the end of the item
                                 // starts, in ms
    int intend_quiet_start_ms; // When does the end of the song start to go
                               // quiet/fade?
    bool blnends_with_fade; // Does the item end with a drawn-out fade?
                            // (otherwise, it ends suddenly)
  } media_info;
  void load_media_info(pg_connection & db);
};

/// A list of programming elements (eg: an announcement batch)
typedef deque <programming_element> programming_element_list;

/// A global variable containing the previous music segment's programming element list
extern programming_element_list prev_music_seg_pel;

// A cache of programming element lists, used when we are in a hurry to get a playlist:
// A class used by generate_playlist to remember recent programming element lists.
class cpel_cache {
  public:
    void clear();
    bool get (const string & id, programming_element_list & pel);
    bool has (const string & id);
    void set (const string & id, const programming_element_list & pel);
  private:
   struct pel_info {
     programming_element_list pel;
     datetime cached_time;
   };
   typedef map <string, pel_info> cache_type;
   cache_type cache;
   void tidy();
};

extern cpel_cache pel_cache; ///< Global variable for player storage of recent programming element lists

#endif
