
#ifndef SEGMENT_H
#define SEGMENT_H

#include "common/my_time.h"
#include "common/psql.h"
#include "programming_element.h"

using namespace std;

// Segment
// Definition: A time slot in an hour broadcast. It can have varying categories. eg: in a
// segment, 15:00 - 17:00, I could have a news segment. But in the news segment I could allow
// advertising to be inserted. A segment is not the same as a production element. A segment can
// contain different categories. (This is a new term we've coined, ie not in regular radio usage).
// The sections in the pie chart earlier are all segments.

class segment {
public:
  segment();  // Constructor
  ~segment(); // Destructor
  void reset(); // Reset all segment info
  void load_from_db(pg_connection & db, const long lngfc_seg, const string & strdefault_music_source, const datetime dtmtime);
  void setup_as_music_profile(const string & strmusic_source, const string & strdesc, pg_connection & db);

  // Advance to the next item (if necessary) and then return it.
  void get_next_item(programming_element & pe, pg_connection & db, const string & strdefault_music_source, const int intstarts_ms);

  bool blnloaded; // Has data been loaded into this object yet?

  // Information about the format clock:
  struct fc {
    long lngfc;     // Database reference
    string strname; // Format Clock name.
  } fc;

  //  Information about the category & alternative category:
  struct cat {
    seg_category cat; // A symbolic enum name ... faster processing etc
    long lngcat;      // Database reference to a  tlkfc_cat record
    string strname;   // Name of the category
  } cat, alt_cat;

  // Information about the sub-category an alternative sub-category:
  struct sub_cat {
    string strsub_cat; // Database reference to a tlkfc_sub_cat record
    string strname;  // Name of the sub-category
    string strdir;   // Directory where sub-category files are found.
  } sub_cat, alt_sub_cat;

  // Segment-specific info:
  long lngfc_seg; // Database id (reference to a tlkfc_seg record)
  enum seg_sequence {
    SSEQ_UNKNOWN,
    SSEQ_RANDOM,
    SSEQ_SEQUENTIAL,
    SSEQ_SPECIFIC
  } sequence;
  string strspecific_media; // Media to play if the sequence is SS_SPECIFIC
  bool blnpromos;           // Promos allowed in this segment?
  bool blnmusic_bed;        // Does this segment have underlying music?
  struct sub_cat music_bed; // Information about the underlying music.
  bool blncrossfading;      // Crossfade music & announcements in this segment?
  bool blnmax_age;          // Does this segment limit the maximum age of sub-category media played?
  int intmax_age;           // If so, this is the maximum age.
  bool blnpremature;        // Ignore the "Relevant from" setting of sub-category media
  bool blnrepeat;           // Repeat sub-category media in this segment?
  int intmax_items;         ///< Maximum number of items allowed to play during this segment;

  // Current state (playing from category, alternate category, or default music profile)
  enum playback_state {
    PBS_CATEGORY,          // Revert from one to the next, etc.
    PBS_ALTERNATE,
    PBS_DEFAULT_MUSIC
  } playback_state;

  struct scheduled { // The full date & time
    datetime dtmstart; // Time that this segment was scheduled to start at
    datetime dtmend;   // Time that this segment was scheduled to end at
  } scheduled;

  // Playback timing:
  int intlength; // Length (seconds) that this segment is meant to play for. Calculated after "load_from_db" is called
  datetime dtmstart; // Time when this segment actually starts playing back (we try to keep our segment length constant, regardless of actual start time).

  /// List of items to play during this segment.
  programming_element_list programming_elements;
private:
  // Information used to retrieve the "next" item:
  programming_element_list::iterator next_item; ///< A pointer to the next item to be returned (if valid etc) by get_next_item

  bool blnfirst_fetched; ///< Set to true when the first item is fetched. Helps
                         ///< logic for navigating the segment items.

  int intnum_fetched;   ///< Number of items fetched so far. Used with intmax_items
                        ///< to limit the number of items played in a segment.

  // Functions which are used to operate on the above:
  void generate_playlist(programming_element_list & pel, const string & strsource, const seg_category pel_cat, pg_connection & db); // strsource is a playlist, directory, etc.

  // Shuffle a programming element list:
  void shuffle_pel(programming_element_list & pel);

  // Function called by load_from_db: Prepare a list of programming elements to use, based on the segment parameters.
  void load_pe_list(programming_element_list & pel, const struct cat & cat, const struct sub_cat & sub_cat, pg_connection & db);

  // If there is a problem with playing category items, we revert to alternate category. If there is also a problem
  // with the alternate category, we attempt to revert to the default music profile. If there are still problems
  // we throw an exception. This function is called to revert from the current playback status to the next lower.
  void revert_down(pg_connection & db, const string & strdefault_music_source);

  // Functions called by load_from_db:
  seg_category parse_category_string(const string & strcat);
  seg_sequence parse_sequence_string(const string & strseq);
  void load_sub_cat_struct(struct sub_cat & sub_cat, const string strsub_cat, pg_connection & db, const struct cat & cat, const long lngfc_seg, const string & strdescr, const string & strfield);

  // A recursive function used to load m3u files that contain directories, and directories which contain m3us:
  // Also applies special logic to format clock sub-category directories
  void recursive_add_to_string_list(vector <string> & file_list, const string & strsource, const int intrecursion_level, pg_connection & db);

  // Cached list of music bed items to use during this segment:
  vector <string> music_bed_media;
  vector <string>::const_iterator music_bed_media_it;
  void list_music_bed_media(pg_connection & db); // Setup the music_bed_media list (and shuffle)
  string get_music_bed_media();
};

#endif
