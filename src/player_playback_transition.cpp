
#include "player.h"
#include <sys/time.h>
#include "common/linein.h"
#include "common/maths.h"
#include "common/my_string.h"
#include "common/string_splitter.h"
#include "common/system.h"
#include "common/rr_misc.h"
#include <fstream>

#include "common/testing.h"

namespace xmmsc = xmms_controller;

void player::playback_transition(playback_events_info & playback_events) {
  // Go into an intensive timing section (5 checks every second) until we're done with the playback event. Logic for
  // transitioning between items, and introducing or cutting off underlying music, etc.

  // Possible types of transitions:
  // 1) Item ending
  // 2) Music bed starts
  // 3) Music bed ends
  // 4) Promo interrupts current playback.

  struct timeval tvprev_now; // Used for checking if the system time moves backwards (eg: ntpdate)
  gettimeofday(&tvprev_now, NULL);

  // Variable used to check if any upcoming events that need to be waited for & handled.
  int intnext_playback_safety_margin_ms = get_next_playback_safety_margin_ms();

  // Check for any music bed events (they are not yet supported!)
  if ((playback_events.intmusic_bed_starts_ms < intnext_playback_safety_margin_ms) ||
      (playback_events.intmusic_bed_ends_ms < intnext_playback_safety_margin_ms)) LOGIC_ERROR;

  // Main loop for this function. Check if there are any nearby events for this item, to wait for and handle:
  while (playback_events.intnext_ms < intnext_playback_safety_margin_ms) {
    // We have 1 or more item playback events coming up in the near future. Prepare for them.

    // Our queue is based on the present point in time. If for example it takes 5 seconds to
    // setup the queue, then the logic will treat the queue as if it is 5 seconds late.
    struct timeval tvqueue_start;
    gettimeofday(&tvqueue_start, NULL);

    // Declare a list of events to be processed:
    transition_event_list events;
    events.clear();

    bool blncrossfade = false; // Set to true if the transition being processed involves a crossfade.
                               // This is important for knowing when to queue music bed events that
                               // will take place before the current item ends (next_becomes_curret).

    int intnext_becomes_current_ms = -1; // Will be set to the exact time in the playlist when the
                                         // next item becomes the current item. This is important
                                         // for being able to time music events

    // How long until the current item ends? (either naturally or due to interruption by a promo,
    // or perhaps due to the user sending an RPLS (reload playlist) command and the current item
    // is not in the playlist:
    int intitem_ends_ms = playback_events.intitem_ends_ms;
    intitem_ends_ms = MIN(intitem_ends_ms, playback_events.intpromo_interrupt_ms);
    intitem_ends_ms = MIN(intitem_ends_ms, playback_events.intrpls_interrupt_ms);
    intitem_ends_ms = MIN(intitem_ends_ms, playback_events.inthour_change_interrupt_ms);

    // And set a flag if our current item is ging to be interrupted:
    bool blninterrupt = intitem_ends_ms != playback_events.intitem_ends_ms;

    // Current item going to end soon? (either naturally, or due to interruption by a promo)
    if (intitem_ends_ms < intnext_playback_safety_margin_ms) {
      // Yes. Prepare for a transition between the current item and the next item

      // At this point, if the previous (current) item was a promo, we mark it as complete now:
      if (run_data.current_item.blnloaded && run_data.current_item.promo.lngtz_slot != -1) {
        mark_promo_complete(run_data.current_item.promo.lngtz_slot);
      }

      // Queue a transition over to the next item:

      // Fetch the next item now if it isn't already loaded:
      if (!run_data.next_item.blnloaded) {
        // Next item is not already known. Switch over:
        get_next_item(run_data.next_item, intitem_ends_ms);
      }

      // Are crossfades allowed now?

      // Crossfades take place:
      // 1) Never if the current item is not available (ie, the player has just started, meaning
      //    that we have the next item but not the current item at this time).
      // 2) Never when the current or next item is a promo
      // 3) Never when going between Linin and Linein
      // 4) Never when going between 2 CD tracks
      // 5) Never when the current item is being interrupted to play something else
      //
      //    OTHERWISE:
      //
      // 6) The current segment allows crossfades AND
      // 7) The 2 items have the same category as the segment (ie: Not for promos that happen to
      //    play during a music segment)

      // Break up the logic:
      {
        // Is this or the next item a promo?
        bool blnthis_or_next_is_promo = run_data.current_item.cat == SCAT_PROMOS ||
                                        run_data.next_item.cat    == SCAT_PROMOS;

        // Do both items have the same catagory as the segment?
        bool blnitem_categories_match_segment  =
               run_data.current_item.cat == run_data.current_segment.cat.cat &&
               run_data.next_item.cat    == run_data.current_segment.cat.cat;

        // Transitioning between LineIn and LineIn?
        bool blnlinein_to_linein_transition =
               run_data.current_item.strmedia == "LineIn" &&
               run_data.next_item.strmedia    == "LineIn";

        // Transitioning between 2 CD tracks?
        bool blncdtrack_to_cdtrack_transition =
               file_is_cd_track(run_data.current_item.strmedia) &&
               file_is_cd_track(run_data.next_item.strmedia);

        // Now put it all together:
        blncrossfade =
          // 1) Never if the current item is not available
          run_data.current_item.blnloaded &&

          // 2) Never if this or the next item is a promo
          !blnthis_or_next_is_promo &&

          // 3) Never when transitioning between LineIn and LineIn:
          !blnlinein_to_linein_transition &&

          // 4) Never when going between 2 CD tracks:
          !blncdtrack_to_cdtrack_transition &&

          // 5) Never when the current item is being interrupted to play something else
          !blninterrupt &&

          // OTHERWISE:
          // 6) The current segment allows crossfades AND
          run_data.current_segment.blncrossfading &&

          // 7)  The 2 items have the same category segment
          blnitem_categories_match_segment;

        // TEMPORARY HACK: Always crossfade if one of the items is music **AND** we're not interrupting the current item
        // (Ugly, Ugly hack!):
        if ((run_data.current_item.cat == SCAT_MUSIC ||
            run_data.next_item.cat == SCAT_MUSIC) && !blninterrupt) {
            log_line("HACK: One of the items is music, so crossfading");
          blncrossfade = true;
          // Hack on top of that one: If the current item's ends suddenly
          // (as determined by rrmedia-maintenance), then we don't
          // crossfade. The next item will start immediately after the
          // current item ends, instead.
          if (run_data.current_item.end.blnloaded &&
              !run_data.current_item.end.blnends_with_fade) {
            log_line("HACK: Nevermind. The current item ends suddenly, so we won't crossfade");
            blncrossfade = false;
          }
          // Another hack to this evilness:
          // - Stefan: Don't crossfade if one of the items (current or next)
          //           is an advert [aka promo]:
          if (blncrossfade &&
              (run_data.current_item.cat == SCAT_PROMOS ||
                run_data.next_item.cat == SCAT_PROMOS)) {
            log_line("HACK: Nevermind. One of the items is a promo, so we won't crossfade");
            blncrossfade = false;
          }
        }

        // Log whether we are going to crossfade:
        if (blncrossfade)
          log_line("Will crossfade between this item and the next.");
        else
          log_line("Will not crossfade between this and the next item.");
      }

      // Script the transition:
      {
        // Determine [next_item_start]
        int intnext_item_start_ms = -1;
        // Is the current item available?
        if (run_data.current_item.blnloaded) {
          // Yes. Are we going to crossfade?
          if (blncrossfade) {
            // Yes. Next item starts just after [current item end] - [crossfade length]
            int intcrossfade_length = config.intcrossfade_length_ms;
            // If the current item is a song, and it fades out gradually, then
            // we should overlap for 1 second only (as per Stefan's request)
            // instead of whatever the player is configured for
            if (run_data.current_item.cat == SCAT_MUSIC &&
                run_data.current_item.end.blnloaded &&
                run_data.current_item.end.blnends_with_fade &&
                config.intcrossfade_length_ms != 1000) {
              log_message("The current item is a song which fades out gradually, so I will crossfade for 1000 ms instead of the configured " + itostr(config.intcrossfade_length_ms) + " ms");
              intcrossfade_length = 1000;
            }
            intnext_item_start_ms = intitem_ends_ms - intcrossfade_length + 1;
          }
          else {
            // No. Next item starts just after the current item ends

            // Is the current item a song which ends suddenly?
            if (run_data.current_item.cat == SCAT_MUSIC &&
                run_data.current_item.end.blnloaded &&
                !run_data.current_item.end.blnends_with_fade &&
                run_data.current_item.end.blndynamically_compressed) {
              // Yes. Start the next song in half a second, as requested by Stefan:
              log_message("Current song ends suddenly, so the next item will start 500 ms after it ends");
              intnext_item_start_ms = intitem_ends_ms + 500;
            }
            else {
              // Nope. Start the next song immediately:
              intnext_item_start_ms = intitem_ends_ms + 1;
            }
          }
        }
        else {
          // Current item is not available. Next item starts immediately.
          intnext_item_start_ms = intitem_ends_ms + 1;
        }

        // Keep track of whether we fade or not (for logging purposes)
        bool blnfade = false;

        // Queue a volume fade out for the current item (if applicable)
        //  - Only if we have the current item
        //  - Only if the current item is music
        //  - Not if the next item is music but crossfading is disabled
        //  *evil hack*: Always when interrupting an item (even if the item is not music)
        if (run_data.current_item.blnloaded &&
            blninterrupt || 
             (run_data.current_item.cat == SCAT_MUSIC &&
             !(run_data.next_item.cat == SCAT_MUSIC && !blncrossfade))) {
          // Setup the current item fade-out:
          // This starts at ([current item end] - [crossfade length]), and continues for [crossfade length]
          // HACK: No fade-outs:
          log_line("HACK: No fading out allowed.");

          // HACK the HACK:
          // Except when we're interrupting the current item
          // (argh: Seriously need to work on the crossfade logic. This is *really* messy)
          if (blninterrupt) {
            log_line("HACK: Nevermind, we're interrupting the current item, so fading it out");
            queue_volslide(events, "current", 100, 0, intitem_ends_ms - config.intcrossfade_length_ms, config.intcrossfade_length_ms);
            if (!blncrossfade) log_message("The current item will fade out");
            blnfade = true; // This transition includes a fade
          }

          // Another hack: If the item is music and fades out by itself over a
          // longish period, then it will be too quiet to hear over the last part
          // so we need to fade it out earlier.
          if (!blnfade &&
              run_data.current_item.cat == SCAT_MUSIC &&
              run_data.current_item.end.blnloaded &&
              run_data.current_item.end.blnends_with_fade) {
            log_line("HACK: Nevermind. It looks like the current item is a song with a long, drawn-out fade, so we will fade it out for 2.5s just before it starts going quiet");
            queue_volslide(events, "current", 100, 0, intitem_ends_ms - 2500, 2500);
            if (!blncrossfade) log_message("The current item will fade out");
            blnfade = true; // This transition includes a fade
          }

//          queue_volslide(events, "current", 100, 0, intitem_ends_ms - config.intcrossfade_length_ms, config.intcrossfade_length_ms);
//          if (!blncrossfade) log_message("The current item will fade out");
//          blnfade = true; // This transition includes a fade
        }

        // Queue a "setup_next_item" event for the next item (eg, claim an xmms session):
        // -> This runs at [next_item_start]
        queue_event(events, "setup_next", intnext_item_start_ms);

        // Queue a volume fade in for the second item (if applicable)
        // - Only if the next item is music
        // - If the current item is not available, or if the current item is not music
        // - Otherwise, if the current item is music, then only if crossfading is enabled
        if (run_data.next_item.cat == SCAT_MUSIC &&
           (blncrossfade || run_data.current_item.cat != SCAT_MUSIC)) {
           // -> This runs at [next_item_start] + 1 ms

           // If the next item plays through linein and the current linein volume is not 0, then set the volume
           // immediately, with no fade-in:
           if (run_data.next_item.strmedia == "LineIn" && linein_getvol() != 0) {
             queue_event(events, "setvol_next 100", intnext_item_start_ms + 1);
             log_warning("LineIn volume was not 0! Will set it to full instead of fading it in");
           }
           else {
             // Otherwise, queue a volume slide (whether for linein or for XMMS):
             // HACK: Set the next item to full volume, don't fade it in
             log_line("HACK: Next item starts at full volume");
             queue_event(events, "setvol_next 100", intnext_item_start_ms + 1);

             //queue_volslide(events, "next", 0, 100, intnext_item_start_ms + 1, config.intcrossfade_length_ms);
             //if (!blncrossfade) log_message("The next item will fade in");
             //blnfade = true; // This transition includes a fade
           }
        }

        // Queue a playback start for the next item (if applicable) (should be just after the fade-in starts, if there is one)
        // - Only if the item is xmms-based. LineIn-based music works by raising & lowering the linein volume.
        if (run_data.next_item.strmedia != "LineIn") {
          // - This runs at [next_item_start] + 2 ms
          queue_event(events, "next_play", intnext_item_start_ms + 2);
        }

        // Queue a "log playback started" event for the 2nd item
        //- This runs at [next_item_start] + 3 ms
        queue_event(events, "log_next_started", intnext_item_start_ms + 3);

        // Log a message if no fade will take place during this transition:
        if (!blncrossfade && !blnfade) log_message("No fades during this transition");

        // Add an event to stop the current item (if it's currently loaded).
        // This is now necessary because we need to end some songs before XMMS
        // finishes playing them (eg, when the last few seconds are silent
        if (run_data.current_item.blnloaded) {
          queue_event(events, "stop_current_item", intitem_ends_ms);
        }

        // Sort the queue.
        sort (events.begin(), events.end(), transition_event_less_than);

        // Add a "next becomes current" event which takes place, after all the other events:
        // - This releases the resources of the "current" item, and switches the "next" item over to
        //   the "current" item
        {
          transition_event last_event = events.back();
          queue_event(events, "next_becomes_current", last_event.intrun_ms + 1);
        }
      }
    }

    // These are used to track the volumes set by "setvol_next" and "setvol_current". Needed so we know what
    // volume to set when we process "current_music_bed_start" and "next_music_bed_start" commands.
    int intvol_current = 100;
    int intvol_next    = 100;

    // Now we have our queue of things to do in the near future. Start a loop where we go through these
    // things.
    transition_event_list::const_iterator current_event = events.begin();
    while (current_event != events.end()) {
      // Fetch the current time:
      timeval tvnow;
      gettimeofday(&tvnow, NULL);

      // Check that the previous recorded time is less than or equal to the current time:
      if (tvprev_now.tv_sec < tvprev_now.tv_sec || (tvprev_now.tv_sec == tvprev_now.tv_sec && tvprev_now.tv_usec < tvprev_now.tv_usec)) {
        testing_throw;
        // System time has moved backwards! Possibly an ntpdate...
        // Calculate how far back the time has moved back.
        timeval tvdiff = tvprev_now;
        tvdiff.tv_sec  -= tvnow.tv_sec;
        tvdiff.tv_usec -= tvnow.tv_usec;
        normalise_timeval(tvdiff);
        log_warning("Detected: System clock moved back " + itostr(tvdiff.tv_sec) + " seconds and " + itostr(tvdiff.tv_usec) + " usecs. Optimistically adjusting playback timing back by this amount.");
        tvqueue_start.tv_sec  -= tvdiff.tv_sec;
        tvqueue_start.tv_usec -= tvdiff.tv_usec;
      }
      tvprev_now = tvnow; // Setup for the next check.

      // Calculate when the next event is to be run:
      timeval tvrun_event;
      tvrun_event = tvqueue_start;
      tvrun_event.tv_usec += current_event->intrun_ms * 1000;
      normalise_timeval(tvrun_event);

      // Calculate difference in usecs:
      timeval tvdiff = tvrun_event;
      tvdiff.tv_sec  -= tvnow.tv_sec;
      tvdiff.tv_usec -= tvnow.tv_usec;
      normalise_timeval(tvdiff);

      long lngusec_diff = tvdiff.tv_sec * 1000000 + tvdiff.tv_usec;

      // If the event is still coming, then wait for it:
      if (lngusec_diff > 0) {
        usleep(lngusec_diff);
      }

      // Now handle the event:
      {
        if (blndebug) cout << "[" << current_event->intrun_ms << "] " << current_event->strevent << endl;
        // Fetch the main command, and any argument from the event string:
        string strcmd = "";
        string strarg = "";
        {
          // Do the splitting here.
          string_splitter event_split(current_event->strevent);

          // Check split result:
          if (event_split.size() < 1 || event_split.size() > 2) LOGIC_ERROR;

          // Fetch main command, and an arg if present.
          strcmd = event_split[0];
          if (event_split.size() == 2) {
            strarg = event_split[1];
          }
        }

        // Handle the various commands:
        if (strcmd == "setup_next") {
          // Here we fetch an available XMMS session, or LineIn. If we're using XMMS then
          // also load the item into XMMS, setup the volume, etc.

          // Is the next_item loaded?
          if (!run_data.next_item.blnloaded) LOGIC_ERROR;

          // Are  there already XMMS or LineIn sessions allocated for the next item? (background or foreground)
          if (run_data.sound_usage_allocated(SU_NEXT_FG) || run_data.sound_usage_allocated(SU_NEXT_BG)) LOGIC_ERROR;

          // Nope. So setup either XMMS or LineIn:
          // * Only run this logic if the item is not in a silence segment.
          if (run_data.next_item.cat != SCAT_SILENCE) {
            if (run_data.next_item.strmedia == "LineIn") {
              // Next item will play through linein.
              // Is LineIn already allocated to something else?
              if (run_data.linein_usage != SU_UNUSED) log_message("LineIn is already used for something, but commandeering it anyway for the next item.");
              run_data.linein_usage = SU_NEXT_FG;
            }
            else {
              // Next item will play through XMMS.
              // Does the item exist? (or is it a CD Track?)
              if (!file_exists(run_data.next_item.strmedia) && !file_is_cd_track(run_data.next_item.strmedia)) my_throw("File not found! " + run_data.next_item.strmedia);

              // - Fetch a free XMMS session
              int intsession = run_data.get_free_xmms_session(); // Will throw an exception if there aren't any free.
              // - Reserve the session for next item/foreground:
              run_data.set_xmms_usage(intsession, SU_NEXT_FG);
              // - Populate the XMMS session:
              xmmsc::xmms[intsession].playlist_clear();
              xmmsc::xmms[intsession].playlist_add_url(run_data.next_item.strmedia);

              // - Set XMMS volume to the next item's volume:
              xmmsc::xmms[intsession].setvol(get_pe_vol(run_data.next_item.strvol));

              // - Make sure that repeat is turned off.
              xmmsc::xmms[intsession].setrepeat(false);
            }
          }
        }
        else if (strcmd == "setvol_next" || strcmd == "setvol_current") {
          // Here we set the output volume (linein or XMMS) to a % of the total volume it wants
          // to play at. Used for volume slides.

          // Setup variables to use here, depending on if we are setting the "next" or "current" volume
          programming_element * item = NULL; // Item to use for checking
          sound_usage SU_FG = SU_UNUSED; // Foreground sound usage for the item
          sound_usage SU_BG = SU_UNUSED; // Background sound usage for the item
          int * intvol      = NULL;      // Variable used for tracking the value set by this command.

          if (strcmd=="setvol_next") {
            // Next item:
            item   = &run_data.next_item;
            SU_FG  = SU_NEXT_FG;
            SU_BG  = SU_NEXT_BG;
            intvol = &intvol_next;
          }
          else {
            // Current item:
            item = &run_data.current_item;
            SU_FG = SU_CURRENT_FG;
            SU_BG = SU_CURRENT_BG;
            intvol = &intvol_current;
          }

          // - Item loaded?
          if (!item->blnloaded) LOGIC_ERROR;

          // Only run the rest of the logic if the item does not have a "Silence" category:
          if (item->cat != SCAT_SILENCE) {
            // Fetch the percentage to use (an error will get thrown if there is a problem):
            int intpercent = strtoi(strarg);

            // Check the range:
            if (intpercent < 0 || intpercent > 100) my_throw("Invalid volume!");

            // Remember this percentage for any "start_music_bed_current" or "start_music_bed_next" commands:
            *intvol = intpercent;

            // - LineIn or XMMS?
            if (item->strmedia == "LineIn") {
              // LineIn.
              // Check: No music beds allowed with LineIn:
              if (run_data.sound_usage_allocated(SU_BG)) my_throw("Music Bed was allocated for LineIn music!");

              // Check: LineIn is allocated to this item:
              if (!run_data.uses_linein(SU_FG)) my_throw("LineIn was not allocated!");

              // Set the volume of linein appropriately:
              linein_setvol((store_status.volumes.intlinein * intpercent)/100);
            }
            else {
              // Set XMMS volume:
              // Fetch session used for the foreground. Will throw an exception if it isn't allocated.
              int intsession = run_data.get_xmms_used(SU_FG);
              // Set volume appropriately:
              xmmsc::xmms[intsession].setvol((get_pe_vol(item->strvol) * intpercent)/100);
              // Also set the XMMS pre-amp:
              xmmsc::xmms[intsession].set_eq_preamp(store_status.volumes.dblxmmseqpreamp);

              // Set volume of music bed also if it is active now:
              {
                int intsession = -1;
                try {
                  intsession = run_data.get_xmms_used(SU_BG);
                } catch(...) {
                  // There is no music bed XMMS session allocated at this time. Do nothing.
                }
                if (intsession != -1) {
                  testing_throw;
                  // We have an XMMS session for the music bed.
                  // Extra check: Does the item actually have a music bed?
                  if (!item->blnmusic_bed) LOGIC_ERROR;
                  xmmsc::xmms[intsession].setvol((get_pe_vol(item->music_bed.strvol) * intpercent)/100);
                }
              }
            }
          }
        }
        else if (strcmd == "next_play") {
          // Skip this if the next item has a "Silence" category:
          if (run_data.next_item.cat != SCAT_SILENCE) {
            // Not relevant for LineIn. If we're using XMMS, then:
            // - Tell XMMS (for the next item) to start playing
            // - Check for music bed events that will occur *during* the current fade (if any) and queue them.

            // Is the next item loaded?
            if (!run_data.next_item.blnloaded) LOGIC_ERROR;

            // Next item can't be linein:
            if (run_data.next_item.strmedia == "LineIn") my_throw("Don't use next_play commands for LineIn!");

            // Fetch the XMMS session:
            int intsession = run_data.get_xmms_used(SU_NEXT_FG);

            // Is XMMS already playing?
            if (xmmsc::xmms[intsession].playing()) my_throw("Don't use next_play when XMMS is already running!");

            // Start it playing now.
            xmmsc::xmms[intsession].play();

            // Create a text file listing the new xmms session number. This is used by
            // the rrxmms-status tool.
            {
              string strfile = "/tmp/.player_active_xmms_session.txt";
              ofstream output_file(strfile.c_str());
              if (!output_file) {
                log_warning("Could not open file for writing: " + strfile);
              }
              else {
                output_file << itostr(intsession) << endl;
              }
            }

            // Check if there are any music bed events (for the next item) that will occur *during* the current
            // fade (ie, before we switch over completely to the next item), and queue them here.
            if (blncrossfade && run_data.next_item.blnmusic_bed) {
  testing_throw;
              if (blndebug) cout << "Queuing any music bed events (for the next item), during the upcoming crossfade..." << endl;
              // Work out the current time in ms, compared to when the queue started:
              timeval tvnow;
              gettimeofday(&tvnow, NULL);
              tvnow.tv_sec  -= tvqueue_start.tv_sec;
              tvnow.tv_usec -= tvqueue_start.tv_usec;
              normalise_timeval(tvnow);
              int intnow_ms = tvnow.tv_sec * 1000 + tvnow.tv_usec / 1000; // How many ms since the queue started.

              // Queue a "next_music_bed_start" if it does start in the near future...
              if (run_data.next_item.music_bed.intstart_ms < intnext_becomes_current_ms) {
  testing_throw;
                queue_event(events, "next_music_bed_start", intnow_ms + 1 + run_data.next_item.music_bed.intstart_ms);
              }

              // Queue a "next_music_bed_end" if it does end in the near future...
              if (run_data.next_item.music_bed.intstart_ms + run_data.next_item.music_bed.intlength_ms < intnext_becomes_current_ms) {
  testing_throw;
                queue_event(events, "next_music_bed_stop", intnow_ms + 2 + run_data.next_item.music_bed.intstart_ms + run_data.next_item.music_bed.intlength_ms);
              }

              // Now sort the list of events:
              sort(events.begin(), events.end(), transition_event_less_than);

              testing_throw; // Check if we are at the same position in the queue as before!
            }
          }
        }
        else if (strcmd == "log_next_started") {
          // Log that the next item has just started:
          run_data.next_item.dtmstarted = now();

          // Find out the XMMS session the next item uses (if any)
          int intxmms_session = -1; // -1 means the next item does not use XMMS
          try {
            // run_data.get_xmms_used() will throw an exception if the next item is not using
            // an XMMS session.
            intxmms_session = run_data.get_xmms_used(SU_NEXT_FG);
          } catch(...) {} // A

          // If the next item uses an XMMS session, then log some XMMS-related info about the next item:
          if (intxmms_session != -1) {
            // Fetch the song length:
            // - First sleep for 1/10th of a second. XMMS's get_song_length() gives a
            // confused output if you call it too soon after play().
            sleep_ms(100);
            int intlength = xmmsc::xmms[intxmms_session].get_song_length();
            char chlength[10];
            sprintf(chlength, "%d:%02d", intlength/60, intlength%60);
            log_message("Playing (xmms " + itostr(intxmms_session) + ": " + itostr(get_pe_vol(run_data.next_item.strvol)) + "%): \"" + xmmsc::xmms[intxmms_session].get_song_file_path() + "\" - \"" + xmmsc::xmms[intxmms_session].get_song_title() + "\" (" + (string)chlength + ". Ends: " + format_datetime(now() + intlength, "%T") + ")");

            // Also update the music history if the next item is a music item:
            if (run_data.next_item.cat == SCAT_MUSIC) {
              m_music_history.song_played(db, run_data.next_item.strmedia, mp3tags.get_mp3_description(run_data.next_item.strmedia));
            }
          }

          // Also, log the current playback status to tblplayeroutput & tblliveinfo:
          log_mp_status_to_db(SU_NEXT_FG);

          // Let the next segment know that one of it's items was played
          // (eg, if we're skipping through items because they were recently played,
          // then don't count them towards # items played (eg, music segment only
          // allows 3 items to play, and MP3s in the playlist have already
          // played recently)
          run_data.current_segment.item_played();
        }
        else if (strcmd == "stop_current_item") {
          // This only really applies to XMMS
          if (!run_data.current_item.blnloaded) LOGIC_ERROR;
          if (run_data.current_item.strmedia != "LineIn") {
            // If not playing linein then playing XMMS.
            int intsession = run_data.get_xmms_used(SU_CURRENT_FG);
            if (xmmsc::xmms[intsession].playing()) {
              // How long until XMMS would normally finish playing the song?
              int intxmms_song_pos_ms    = xmmsc::xmms[intsession].get_song_pos_ms();
              int intxmms_song_length_ms = xmmsc::xmms[intsession].get_song_length_ms();
              int inttime_ms = intxmms_song_length_ms - intxmms_song_pos_ms;
              // Now log the stopping message:
              log_line("Stopping XMMS session " + itostr(intsession) + " (" + itostr(inttime_ms) + " ms before the MP3 end)");
              xmmsc::xmms[intsession].stop();
            }
            else {
              log_line("XMMS session " + itostr(intsession) + " stopped by itself");
            }
            // Now mark the stopped XMMS session as unused, so it can be re-used:
            run_data.set_xmms_usage(intsession, SU_UNUSED);
          }
        }
        else if (strcmd == "next_becomes_current") {
          // Current item has just ended. It is no longer interesting in terms of player timing. So
          // we the "next" item (which we have just finished transitioning into) becomes the "current" item.

          // Now switch over resource usage:
          run_data.next_becomes_current();

          // Also the local variables we're using to track volumes (for current_music_bed_start, next_music_bed_start commands):
          intvol_current = intvol_next;
          intvol_next = 100;
        }
        else if (strcmd == "current_music_bed_start" || strcmd == "next_music_bed_start") {
          // Variables we use for our logic:
          sound_usage SU_BG = SU_UNUSED;
          programming_element * pe  = NULL;
          int intvol = 0;
          // Setup variables;
          if (strcmd == "current_music_bed_start") {
            // Current item's Music Bed: Start
            SU_BG=SU_CURRENT_BG;
            pe = &run_data.current_item;
            intvol = intvol_current;
          }
          else {
            // Next item's Music Bed: Start
            testing_throw;
            SU_BG=SU_NEXT_BG;
            pe = &run_data.next_item;
            intvol = intvol_next;
          }
          // Now run our logic for starting a music bed:

          // Check if there is already an XMMS session reserved:
          bool blnfound = false;
          try {
            run_data.get_xmms_used(SU_BG); // This should throw an exception, we don't have a current background yet.
            testing_throw;
            blnfound = true;
          } catch(...) {};
          if (blnfound) LOGIC_ERROR;

          // Fetch an available XMMS session
          int intsession = run_data.get_free_xmms_session(); // Will throw an exception if there aren't any free
          run_data.set_xmms_usage(intsession, SU_BG);

          // Setup the session
          xmmsc::xmms[intsession].playlist_clear();
          xmmsc::xmms[intsession].playlist_add_url(pe->music_bed.strmedia);
          xmmsc::xmms[intsession].setvol((get_pe_vol(pe->music_bed.strvol)*intvol)/100);
          xmmsc::xmms[intsession].setrepeat(false);

          // Start the session
          xmmsc::xmms[intsession].play();
          // Log that we're playing underlying music:
          log_message("Music Bed (xmms " + itostr(intsession) + ": " + itostr(get_pe_vol(pe->music_bed.strvol)) + "%): \"" + xmmsc::xmms[intsession].get_song_file_path() + "\" - \"" + xmmsc::xmms[intsession].get_song_title() + "\"");

          // Fetch the length of the music bed according to XMMS:
          {
            int intmusic_bed_length_ms_xmms = xmmsc::xmms[intsession].get_song_length_ms();

            // If the music bed's length is unknown (or was listed as too long in the db), update it here:
            if (pe->music_bed.intlength_ms > intmusic_bed_length_ms_xmms) {
              pe->music_bed.intlength_ms = intmusic_bed_length_ms_xmms;
            }
          }

          // Record that this event has been handled:
          pe->music_bed.already_handled.blnstart = true;
        }
        else if (strcmd == "current_music_bed_stop" || strcmd == "next_music_bed_stop") {
          // Variables we use for our logic:
          sound_usage SU_BG = SU_UNUSED;
          programming_element * pe  = NULL;
          // Setup variables;
          if (strcmd == "current_music_bed_stop") {
            // Current item's Music Bed: Start
            SU_BG=SU_CURRENT_BG;
            pe = &run_data.current_item;
          }
          else {
            // Next item's Music Bed: Start
            testing_throw;
            SU_BG=SU_NEXT_BG;
            pe = &run_data.next_item;
          }
          // Now run our logic for starting a music bed:

          // Fetch the xmms session:
          int intsession = run_data.get_xmms_used(SU_BG);

          // Stop XMMS & free the session:
          xmmsc::xmms[intsession].stop();
          run_data.set_xmms_usage(intsession, SU_UNUSED);

          // Record that this event has been handled:
          pe->music_bed.already_handled.blnstop = true;
        }
        else my_throw("Unknown playback transition command! \"" + strcmd + "\"");
      }
      // Now go to the next event.
      ++current_event;
    }

    // Done with the transitions. Look for any other upcoming events during playback of the current item:
    get_playback_events_info(playback_events, config.intcrossfade_length_ms);
    // - Remember that we could already be part-way into the next item by the time we do this (eg: crossfade transition).
    // - If we already started or stopped a music bed then don't add them again (ie, if they are too far in the past, ignore?
  }
}

// Used by playback_transition():
void player::queue_event(transition_event_list & events, const string & strevent, const int intwhen_ms) {
  if (blndebug) cout << "Queued: [" << intwhen_ms << "] " << strevent << endl;
  // Add an event to the queue.
  // Setup the event:
  transition_event event;
  event.intrun_ms = intwhen_ms;
  event.strevent  = strevent;

  // Add it to the queue:
  events.push_back(event);
}

void player::queue_volslide(transition_event_list & events, const string & strwhich_item, const int intfrom_vol_percent, const int intto_vol_percent, const int intwhen_ms, const int intlength_ms) {
  // Queue a volume slide from intfrom_vol_percent to intto_vol_percent.
  // Check the length of the fade:
  if (intlength_ms == 0) my_throw("Fade length cannot be 0!");

  int intfade_pos_ms=0; // Current position in the fade
  int intlast_vol_percent=-10000; // Used so that we don't queue duplicate setvol commands.
  while (intfade_pos_ms <= intlength_ms) {
    // Build the event to run:
    transition_event event;
    int intvol_percent = intfrom_vol_percent + ((intto_vol_percent - intfrom_vol_percent) * intfade_pos_ms) / intlength_ms;

    // Only finish building & queue the event if the volume has changed since last time:
    if (intvol_percent != intlast_vol_percent) {
      string strevent ="setvol_" + strwhich_item + " " + itostr(intvol_percent);
      int intrun_ms   = intwhen_ms + intfade_pos_ms; // When to queue this event for.

      // And queue the event:
      queue_event(events, strevent, intrun_ms);

      // Remember that we have queued a setvol to this percentage:
      intlast_vol_percent = intvol_percent;
    }

    // Go another 200 ms (1/5th of a second) into the fade:
    intfade_pos_ms+=200;
  }

  // Did we queue the final setvol (can be missed if the crossfade length is not
  // an exact multiple of 200ms:
  if (intlast_vol_percent != intto_vol_percent) {
    // Yes. We skipped the final setvol. Queue it now:
    string strevent ="setvol_" + strwhich_item + " " + itostr(intto_vol_percent);
    queue_event(events, strevent, intwhen_ms + intlength_ms);
  }
}
