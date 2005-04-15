
#ifndef PLAYER_H
#define PLAYER_H

#include "programming_element.h"
#include <string>
#include "common/psql.h"
#include "common/testing.h"
#include "common/xmms_controller.h"
#include "segment.h"

using namespace std;

// Some player-related constants:
const string PLAYER_DIR = "/data/radio_retail/progs/player/";

const int intcrossfade_length_ms             = 8000; // Crossfades run for 8000ms. Also music fade-ins and fade-outs.
const int intnext_playback_safety_margin_ms = 18000; // How long before important playback events, the player should be ready and
                                                     // not run other logic which could cut into time needed for crossfading, etc.
const int intmax_xmms = 4;                           // Number of XMMS's required for playback. 4 would be necessary when
                                                     // crossfading between two items, both with underlying music.
const int intmax_segment_push_back = 6*60;           // Maximum amount of time in seconds that segments will be
                                                     // "pushed back" because previous segments played for too long.


                                                     
// Information about "events" that take place during playback of the current item.
// (current item ends, music bed starts, music bed starts
class playback_events_info {
public:
  playback_events_info(); // Constructor
  void reset(); // Reset attributes back to default values.
  int intnext_ms;      // ms until the next event happens
  int intitem_ends_ms; // ms until the current item ends
  int intmusic_bed_starts_ms; // ms until the music bed starts playing
  int intmusic_bed_ends_ms;   // ms until the music bed stops playing.
  int intpromo_interrupt_ms;  // ms until the item (ie, music) will be interrupted to play a promo.
  programming_element promo;  // If this item is interrupted by a promo then this var is populated
};

// Structure for storing events that take place during a playback transition (see player::playback_transition)
struct transition_event {
  int intrun_ms; // When does the event run?
  string strevent; // Lists the event.
};
typedef vector <transition_event> transition_event_list;
// A function we use with the sort() algorithm:
bool transition_event_less_than(const transition_event & e1, const transition_event & e2);
                                                      
class player {
public:
  player();  // Constructor
  ~player(); // Destructor
  void run(); // Main logic
  void log(const log_info & LI); // Call this to write a log to the player logfile & to the schedule database.
private:
  void init();  // Called by the constructor to do the actual init (kdevelop breakpoint problems).

  // FUNCTIONS AND ATTRIBUTES USED DURING INIT():  
  void reset(); // Reset ALL object attributes to default, uninitialized values.
  void remove_waiting_mediaplayer_cmds(); // Remove waiting MediaPlayer commands (pause, stop, resume, etc)
  void write_liveinfo(); // Write status info to a table for the Global Reporter to read.
  void write_liveinfo_setting(const string strname, const string strvalue);
  void check_received(); // Check the Received directory for .CMD files
  void load_cmd_into_db(const string strfull_path);
  void process_waiting_cmds();
  void correct_waiting_promos();
  void write_errors_for_missed_promos();
  void write_errors_for_missed_promos_log_missed(const string strmissed_file, const long lngmissed_count, const datetime dtmmissed_first, const datetime dtmmissed_last);
  void log_xmms_status_to_db();

  pg_connection db; // Connection to the schedule database. This is used to run queries and fetch records.

  // A callback function called by the db (database connection) object when there is a database
  // connection problem. It keeps music going, etc.
  static void callback_check_db_error();

  // Some settings and configuration read from the config file and from the database.
  struct config {
    // Database connection details (player.conf)
    struct db {
      string strserver;
      string strdb;  // The name of the MySQL database
      string struser;  // The user name
      string strpassword;  // The password
      string strport;  // The port
    } db;

    // Promo frequency capping options (tbldefs)
    int intmins_to_miss_promos_after; // If an announcement was scheduled to play earlier than this amount of time ago, then skip it if it has not already been played.
    int intmax_promos_per_batch;
    int intmin_mins_between_batches; // Minimum amount of music to play between promo batches

    // directories:
    struct dirs {
      string strmp3;
      string stradverts;
      string strannouncements;
      string strspecials;
      string strreceived;
      string strtoday;
    } dirs;

    // Added in 6.15 (build 330) The location to use for the default music profile:
    string strdefault_music_source;

    // Added in 6.21 (build 737) Wait for the current song to end before starting
    // promo playback? Exception: linein music & "force to play now" promos.
    bool blnpromos_wait_for_song_end;

    // New settings added in v7.00
    // Format clock settings:
    bool blnformat_clocks_enabled; // Are format clocks used on this system?
    long lngdefault_format_clock;  // Database reference to the "default" format clock (to use if
                                   // there are problems with the current format clock, or no format clocks
                                   // were scheduled.
  } config;
  
  void read_config_file(); // Read database connection settings from the player config file into the config.db structure.
  void load_db_config();   // Load all the other settings (besides config.db) into the config structure.

  // Load & Save tbldefs settings:
  string load_tbldefs(const string & strsetting, const string & strdefault, const string & strtype);
  void save_tbldefs(const string & strsetting, const string & strtype, const string & strvalue);
 
  struct store_status { // Current store status
     bool blnopen; // Is the store currently open? (compare current time with tblstorehours).
     // These volumes are represented as PERCENTAGES (ie, out of 100, not out of 255).
     // Additionally, these volumes are converted to 80% of their original to prevent distortion of louder
     // levels. After calling "load_store_status()", player logic can use the levels here directly for playback.
     // Volumes in this struct are reset to 0% when the store is closed.
     struct volumes {  // Current volumes, adjusted correctly
       int intmusic;    // Has hourly adjustment vol added
       int intannounce; // Has music volume (adjusted) added.
       int intlinein;   // Just the value from tbldefs
     } volumes;
  } store_status;
  void load_store_status(); // Update the current store status

  // Promo status (used by tblSchedule_TZ_Slot.bitScheduled)
  enum advert_status_type {
    ADVERT_SNS_LOADED     = 0,
    ADVERT_LISTED_TO_PLAY = 1,
    ADVERT_PAUSED         = 2,
    ADVERT_DELETED        = 3,
    ADVERT_PLAYED         = 4
  };

  // FUNCTIONS AND ATTRIBUTES USED DURING RUN():

   // Track what a given sound resource (linein, xmms session) is being used for:
  enum sound_usage {
    SU_UNUSED,     // Not used by an item.
    SU_CURRENT_FG, // Sound resource is busy playing the current item.
    SU_CURRENT_BG, // Sound resource is busy playing the current item's underlying music.
    SU_NEXT_FG,    // Sound resource is busy playing the next item
    SU_NEXT_BG     // Sound resource is busy playing the next item's underlying music
  };
  
  // Sub-class containing "run" data. ie XMMS info, "current" programming element, "next" PE, current segment, etc, etc.
  class run_data {
  public:
    void init(); // Run this to reset/reinitialize playback status.
  
    // Programming elements (current item, next item):
    programming_element current_item; // Current programming element being played. Includes special events, etc.
    programming_element next_item;    // Next programming element to be played.

    // The current Format Clock segment:
    segment current_segment;

    // XMMS control:
    xmms_controller xmms[intmax_xmms]; // XMMS controller instances.

    sound_usage xmms_usage[intmax_xmms]; // What the xmms sessions are being used for
    sound_usage linein_usage; // What LineIn is being used for.

    // Fetch an unused xmms session:
    int get_free_xmms_session();

    // Set the status of an XMMS session:
    void set_xmms_usage(const int intsession, const sound_usage sound_usage);

    // Fetch which XMMS session is being used by a given "usage". eg, item fg, item bg, etc.
    // - Throws an exception if we can't find which XMMS session is being used. Also if LineIn is being used.
    int get_xmms_used(const sound_usage sound_usage);

    // Returns true if linein is being used for the specified usage (foreground, underlying music, etc).
    bool uses_linein(const sound_usage sound_usage);

    // Returns true if there is already a "sound resource" allocated for the specified usage.
    bool sound_usage_allocated(const sound_usage sound_usage);

    // Use this to transition info, resource allocation, etc, etc from "next" to "current".
    void next_becomes_current();

    // How long Format Clock Segments are currently "delayed" by. Segments are "delayed" (ie, they start & end later)
    // when earlier segments take too long to play. This figure will go up between non-music segments, up to the
    // limit (6 mins), and then is reduced when we hit a music segment. Music segments are reduced down to a minimum
    // of 30 seconds to reclaim time back from this "puch back" factor.
    int intsegment_delay;

    programming_element_list waiting_promos; // List of promos waiting to play. Populated by get_next_item_promo
  } run_data;

  // Check playback status of XMMS, LineIn, etc. Throw errors here if there is something wrong.
  // Also compares xmms_usage and linein_usage with the current device status.
  void check_playback_status();

  // Fetch the next item to be played, after the current one. Checks for announcements (promos) to play
  // format clock details, music, linein, etc, etc. Also sets a flag which determines if a crossfade
  // must take place when transitioning to the next item. When we reach the store closing hour, the next
  // item is automatically the "Silence" category (overriding anything else that might want to play now)
  // Also here must come logic for when a) repeat runs out before the segment end, and b) when some time of
  // the next segement is used up by accident (push slots forwards by up to 6 mins, reclaim space by using up music time).   
  void get_next_item(programming_element & item, const int intstarts_ms); // intstarts_ms - how long from now (in ms) the item will be played.

  // Functions called by get_next_item():
    void get_next_item_promo(programming_element & next_item, const int intstarts_ms); // Populate argument with the next promo if there are promos waiting.
    void get_next_item_format_clock(programming_element & next_item, const int intstarts_ms); // Use Format Clocks to determine an item to be played.

  // Functions called by get_item_format_clock:
    long get_fc_segment(const long lngfc, const string & strsql_time);

  // Fetch timing info about events that will take place during playback of the current item
  // (music bed starts, music bed ends, item ends).
  void get_playback_events_info(playback_events_info & event_info, const int intinterrupt_promo_delay);
  
  // Do background maintenance (separate function). Events have frequencies, (sometimes desired "second" to take place at), and are prioritiesed.
  //  - Also includes resetting info about the next playback item (highest priority, every 30 seconds..., seconds: 00, 30)
  void player_maintenance(const int intmax_time_ms);

  // Go into an intensive timing section (5 checks every second) until we're done with the playback event. Logic for
  // transitioning between items, and introducing or cutting off underlying music, etc.
  void playback_transition(playback_events_info & playback_events);
    // Used by playback_transition():
    void queue_event(transition_event_list & events, const string & strevent, const int intwhen_ms);
    void queue_volslide(transition_event_list & events, const string & strwhich_item, const int intfrom_vol_percent, const int intto_vol_percent, const int intwhen_ms, const int intlength_ms);    

  // Fetch actual volumes to use, based on item's "strvol" or "strunderlying_media_vol" settings.
  int get_pe_vol(const string & strpe_vol);

  // Structure used by get_next_item_promo:
  struct TWaitingAnnounce {
    unsigned long dbPos;
    string strFileName;
    string strProductCat;

    datetime dtmTime;
    bool blnForcedTime; // True if dtmTime is a "forced" time. (ie, not a slot in the hour, selected by the sns_loader).
    string strPriority;

    string strPlayAtPercent;
    string strAnnCode;

    string strPath; // The path where this mp3 is found...
  };
  typedef deque <TWaitingAnnounce> TWaitingAnnouncements;


  // This is an internal function used only by playback_transition. It is called when
  // an announcement plays, to log to the database that it has played.
  void mark_promo_complete(const long lngtz_slot);
};

extern player * pplayer; // A pointer to the currently-running player instance. Automatically maintained
                         // by the current player instance. It gets set to NULL when there is no player object.
                         // Use this for callback functions, etc.

#endif
