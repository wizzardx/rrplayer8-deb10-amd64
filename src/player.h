
#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <vector>

#include "music_history.h"
#include "player_config.h"
#include "player_run_data.h"
#include "common/my_time.h"
#include "common/psql.h"

/// Information about "events" that take place during playback of the current item.
class playback_events_info {
public:
  playback_events_info(); ///< Constructor
  void reset();           ///< Reset attributes back to default values.
  int intnext_ms;         ///< ms until the next event happens
  string strnext_descr;   ///< Short description of the next event
  int intitem_ends_ms;    ///< ms until the current item ends
  int intmusic_bed_starts_ms; ///< ms until the music bed starts playing
  int intmusic_bed_ends_ms;   ///< ms until the music bed stops playing.
  int intpromo_interrupt_ms;  ///< ms until the item (ie, music) will be interrupted to play a promo.
  int intrpls_interrupt_ms;   ///< ms until the item (ie, music) will be interrupted to go to the next
                              ///< song (when the user has run an RPLS and the current item is not in the new playlist)
  int inthour_change_interrupt_ms; ///< ms until the item (ie, music) will be interrupted because the hour changed.
};

/// Structure for storing events that take place during a playback transition (see player::playback_transition)
struct transition_event {
  int intrun_ms; ///< When does the event run?
  std::string strevent; ///< Lists the event.
};
typedef std::vector <transition_event> transition_event_list;

/// A function we use with the sort() algorithm:
bool transition_event_less_than(const transition_event & e1, const transition_event & e2);

// Forward declarations:
class log_info;

/// The main Player class. It all happens here
class player {
public:
  player();  ///< Constructor.
  ~player(); ///< Destructor
  void run(); ///< Main logic
private:
  void init();  ///< Called by the constructor to do the actual init (kdevelop breakpoint problems).

  // FUNCTIONS AND ATTRIBUTES USED DURING INIT():
  void reset(); ///< Reset ALL object attributes to default, uninitialized values.
  void remove_waiting_mediaplayer_cmds(); ///< Remove waiting MediaPlayer commands (pause, stop, resume, etc)
  void write_liveinfo(); ///< Write status info to a table for the Global Reporter to read.
  void check_received(); ///< Check the Received directory for .CMD files
  void load_cmd_into_db(const std::string strfull_path);
  void process_waiting_cmds();
  void correct_waiting_promos();
  void write_errors_for_missed_promos();
  void write_errors_for_missed_promos_log_missed(const std::string strmissed_file, const long lngmissed_count, const datetime dtmmissed_first, const datetime dtmmissed_last);
  datetime get_miss_promos_before_time(); // Return the time (no date) before which we start missing promos
  void log_xmms_status_to_db();

  pg_connection db; ///< Connection to the schedule database. This is used to run queries and fetch records.

  /// A callback function called by the db (database connection) object when there is a database
  /// connection problem. It keeps music going, etc.
  static void callback_check_db_error();

  player_config config;

  void read_config_file(); ///< Read database connection settings from the player config file into the config.db structure.
  void load_db_config();   ///< Load all the other settings (besides config.db) into the config structure.

  /// Current store status
  struct store_status {
     bool blnopen; ///< Is the store currently open? (compare current time with tblstorehours).
     /// These volumes are represented as PERCENTAGES (ie, out of 100, not out of 255).
     /// Additionally, these volumes are converted to 80% of their original to prevent distortion of louder
     /// levels. After calling "load_store_status()", player logic can use the levels here directly for playback.
     /// Volumes in this struct are reset to 0% when the store is closed.
     /// Current volumes, adjusted correctly
     struct volumes {
       int intmusic;    ///< Has hourly adjustment vol added
       int intannounce; ///< Has music volume (adjusted) added.
       int intlinein;   ///< Just the value from tbldefs
       double dblxmmseqpreamp; ///< XMMS equalizer pre-amp (some stores need a lot of signal amp)
     } volumes;
  } store_status;
  void load_store_status(const bool blnverbose = false, const bool blnforceload = false); ///< Update the current store status
  void update_output_volumes(); ///< Immediately update XMMS sessions & LineIn levels according to the current store status (don't wait for the next programming element to start playing)

  /// Promo status (used by tblSchedule_TZ_Slot.bitScheduled)
  enum advert_status_type {
    ADVERT_SNS_LOADED     = 0,
    ADVERT_LISTED_TO_PLAY = 1,
    ADVERT_PAUSED         = 2,
    ADVERT_DELETED        = 3,
    ADVERT_PLAYED         = 4
  };

  // FUNCTIONS AND ATTRIBUTES USED DURING RUN():

  /// Sub-class containing "run" data. ie XMMS info, "current" programming element, "next" PE, current segment, etc, etc.
  player_run_data run_data;

  /// Check playback status of XMMS, LineIn, etc.
  /// Throws errors here if there is something wrong.
  /// Also compares xmms_usage and linein_usage with the current device status.
  void check_playback_status();

  // Fetch the next item to be played, after the current one. Checks for announcements (promos) to play
  // format clock details, music, linein, etc, etc. Also sets a flag which determines if a crossfade
  // must take place when transitioning to the next item. When we reach the store closing hour, the next
  // item is automatically the "Silence" category (overriding anything else that might want to play now)
  // Also here must come logic for when a) repeat runs out before the segment end, and b) when some time of
  // the next segement is used up by accident (push slots forwards by up to 6 mins, reclaim space by using up music time).
  void get_next_item(programming_element & item, const int intstarts_ms); // intstarts_ms - how long from now (in ms) the item will be played.

  // Functions called by get_next_item():
    void get_next_item_check_fc_seg_change(const int intstarts_ms); // Call this before the others, to detect FC seg changes.
    void get_next_item_promo(programming_element & next_item, const int intstarts_ms, const bool blnwould_interrupt_song); // Populate argument with the next promo if there are promos waiting.
    void get_next_item_format_clock(programming_element & next_item, const int intstarts_ms); // Use Format Clocks to determine an item to be played.

    // Functions called by get_next_item_format_clock:
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

    // PAYB stuff:
    string strPrerecMediaRef;
    bool blnCheckPrerecLifespan;
  };
  typedef deque <TWaitingAnnounce> TWaitingAnnouncements;

  // This is an internal function used only by playback_transition. It is called when
  // an announcement plays, to log to the database that it has played.
  void mark_promo_complete(const long lngtz_slot);

  // Timed player maintenance events. Run when there is spare time during playback.
  // - Called by player_maintenance();
  void maintenance_check_received(const datetime dtmcutoff);
  void maintenance_check_waiting_cmds(const datetime dtmcutoff);
  void maintenance_operational_check(const datetime dtmcutoff);
  void maintenance_player_running(const datetime dtmcutoff);
  void maintenance_hide_xmms_windows(const datetime dtmcutoff); ///< Hide all visible XMMS windows.
  void maintenance_cache_pels(const datetime dtmcutoff); ///< Pre-generate playlists

  // Functions called by maintenance_operational_check:
  void log_music_playlist_to_db(); ///< Log the contents of the current music playlist to the database

  // Log the current media playback status to the database.
  // Call with a sound_usage of SU_NEXT_FG when you are busy transitioning to the next item
  // and want to get status info for the next item, not the current item.
  void log_mp_status_to_db(const sound_usage sound_usage = SU_CURRENT_FG);

  mp3_tags mp3tags; ///< A cache of mp3 tags, used for quickly retrieving mp3 details.

  // Fetch the current playback safety margin:
  //   How long before important playback events, the player should be ready and
  //   not run other logic which could cut into time needed for crossfading, etc.
  int get_next_playback_safety_margin_ms();

  /// Player music history. For updating tblmusichistory & preventing song repetition.
  music_history m_music_history;
};

extern player * pplayer; // A pointer to the currently-running player instance. Automatically maintained
                         // by the current player instance. It gets set to NULL when there is no player object.
                         // Use this for callback functions, etc.


// Utility functions, split off from the player class to make them more
// easily unit testable.

// Functions called by player::get_next_item_format_clock:

// This function stops inappropriate songs from being played from the
// playlist.
void get_next_ok_music_item(
     programming_element & next_item, const int intstarts_ms,
     music_history & music_history, mp3_tags & mp3tags, pg_connection & db,
     player_config & config, player_run_data & run_data);

#endif
