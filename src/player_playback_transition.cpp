#include "player.h"
#include <sys/time.h>
#include "common/testing.h"
#include "common/linein.h"
#include "common/my_string.h"
#include "common/string_splitter.h"
#include "common/system.h"
#include <fstream>

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

  // Main loop for this function. Check if there are any nearby events for this item, to wait for and handle:
  int intnext_playback_safety_margin_ms = get_next_playback_safety_margin_ms();
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

    // Current item going to end soon?
    if (playback_events.intitem_ends_ms < intnext_playback_safety_margin_ms) {
      // Yes.

      // At this point, if the previous (current) item was a promo, we mark it as complete now:
      if (run_data.current_item.blnloaded && run_data.current_item.promo.lngtz_slot != -1) {
        mark_promo_complete(run_data.current_item.promo.lngtz_slot);
      }

      // Queue a transition over to the next item:

      // Fetch the next item now.
      bool blnsegment_change = false; // We also check if the segment changes
      {
        long lngfc_seg_before = run_data.current_segment.lngfc_seg;

        // Only get the next item if it wasn't previously loaded.
        // This can happen when we're busy playing linein or silence,
        // or if the current music item is being interrupted for a promo.
        if (!run_data.next_item.blnloaded) {
          // Next item is not already known. Switch over:
          get_next_item(run_data.next_item, playback_events.intitem_ends_ms);
        }
        blnsegment_change = lngfc_seg_before != run_data.current_segment.lngfc_seg;
      }

      // Are crossfades allowed now?

      // Crossfades take place:
      // 1) Never if the current item is not available (ie, the player has just started, meaning
      //    that we have the next item but not the current item at this time).
      // 2) Never when the current or next item is a promo
      // 3) Never when going between Linin and Linein (even if in a music segment which allows crossfades)
      //
      //    OTHERWISE:
      //
      // 4) Always when the segment changes, OR
      // 5) The current segment allows crossfades AND
      // 6.1)   The 2 items have the same category as the segment (ie: Not for promos that happen to
      //        play during a music segment), OR
      // 6.2)   When transitioning from a non-music item to a music item, or the other way.
      //        (but not from music -> music. The segment needs to allow it in this case).

      bool blncrossfade = false;

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

        // Transitioning between non-music and music, or the reverse?
        bool blnmusic_non_music_transition = (run_data.current_item.cat == SCAT_MUSIC ||
                                              run_data.next_item.cat == SCAT_MUSIC)
                                             &&
                                             (run_data.current_item.cat != SCAT_MUSIC ||
                                              run_data.next_item.cat != SCAT_MUSIC);

        // Now put it all together:
        blncrossfade =
          // 1) Never if the current item is not available
          run_data.current_item.blnloaded &&

          // 2) Never if this or the next item is a promo
          !blnthis_or_next_is_promo &&

          // 3) Never when transitioning between LineIn and LineIn:
          !blnlinein_to_linein_transition &&

          // OTHERWISE:
          (
            // 4) Always when the segment changes, OR
            blnsegment_change ||
            (
               // 5) The current segment allows crossfades AND
               // 6.1)   The 2 items have the same category as the segment, OR
               // 6.2)   When transitioning between non-music and music (or the reverse)
               run_data.current_segment.blncrossfading &&
                 (blnitem_categories_match_segment || blnmusic_non_music_transition)
            )
          );
      }

      // So, do we crossfade between this item and the next?
      if (blncrossfade) {
        log_line("Will crossfade between this item and the next.");
        // Yes:
        // - This means that for a period of time, two items will be playing together.
        // - When we crossfade music & non-music, only the music volume is shifted. The
        //   non-music item's volume remains constant.

        // Is the current item music?
        if (run_data.current_item.cat == SCAT_MUSIC) {
          // Queue:
          //  1) A volume slide from 100% to 0
          queue_volslide(events, "current", 100, 0, playback_events.intitem_ends_ms - config.intcrossfade_length_ms, config.intcrossfade_length_ms);
        }

        // And transition in the next item:

        // Queue:
        // 1) "setup_next"
        queue_event(events, "setup_next", playback_events.intitem_ends_ms - config.intcrossfade_length_ms + 1);
        // 2) A fade-in (if music)
        if (run_data.next_item.cat == SCAT_MUSIC) {
          queue_volslide(events, "next", 0, 100, playback_events.intitem_ends_ms - config.intcrossfade_length_ms + 2, config.intcrossfade_length_ms);
        }
        // 3) "next_play",
        queue_event(events, "next_play", playback_events.intitem_ends_ms - config.intcrossfade_length_ms + 3);

        // 4) "next_becomes_current"
        queue_event(events, "next_becomes_current", playback_events.intitem_ends_ms + 3);
        intnext_becomes_current_ms = playback_events.intitem_ends_ms+3;
      }
      else {
        // No: Current item will end without a volume fade.
        // If we don't have a current item, (or we do have a current item, and it is not music)
        // and the next item is music, then we fade in the music:
        log_line("Will not crossfade between this item and the next item.");
        if ((!run_data.current_item.blnloaded || run_data.current_item.cat != SCAT_MUSIC) && run_data.next_item.cat == SCAT_MUSIC) {
          // We fade in the next item
          // Queue:

          //  1) "setup_next"
          queue_event(events, "setup_next", playback_events.intitem_ends_ms + 1);

          if (run_data.next_item.strmedia != "LineIn") {
            log_line("The next item will fade in after this item ends.");
            // Next item plays through XMMS:
            //  2) "setvol_next 0" (Only if XMMS)
            queue_event(events, "setvol_next 0", playback_events.intitem_ends_ms + 2);
            //  3) "next_play" (ony if XMMS)
            queue_event(events, "next_play", playback_events.intitem_ends_ms + 3);
            // 3) A volume slide from current vol (if LineIn, or 0 if XMMS) to full music
            queue_volslide(events, "next", 0, 100, playback_events.intitem_ends_ms + 4, config.intcrossfade_length_ms);
          }
          else {
            // Next item plays through Linein
            testing_throw;
            // 3) A volume slide from current vol (if LineIn, or 0 if XMMS) to full music
            if (linein_getvol() == 0) {
              // If linein is 0, we slide it up to full
              queue_volslide(events, "next", 0, 100, playback_events.intitem_ends_ms + 4, config.intcrossfade_length_ms);
            }
            else {
              // If linein is not 0, we set it to full immediately.
              queue_event(events, "setvol_next 100", playback_events.intitem_ends_ms + 4);
            }
          }
          //  4) "next_becomes_current" (transition is over)
          queue_event(events, "next_becomes_current", playback_events.intitem_ends_ms + 5 + config.intcrossfade_length_ms);
          intnext_becomes_current_ms = playback_events.intitem_ends_ms + 5 + config.intcrossfade_length_ms;
        }
        else {
          // Just queue the next item to play when this one ends.
          // Queue:
          log_line("No fading will take place");
          //  1) "setup_next" (Only if XMMS).
          queue_event(events, "setup_next", playback_events.intitem_ends_ms);
          //  2) "setvol_next 100" (100% of the volume it is listed to play at)
          queue_event(events, "setvol_next 100", playback_events.intitem_ends_ms+1);
          //  2) "next_play" (Only if XMMS)
          queue_event(events, "next_play", playback_events.intitem_ends_ms+2);
          //  3) "next_becomes_current" (transition is over)
          queue_event(events, "next_becomes_current", playback_events.intitem_ends_ms+3);
          intnext_becomes_current_ms = playback_events.intitem_ends_ms+3;
        }
      }

//      // Queue a "stop" for the current item's music bed if it has one
//      if (run_data.current_item.blnloaded && run_data.current_item.blnmusic_bed) {
//        testing;
//        queue_event(events, "current_music_bed_stop", intnext_becomes_current_ms-1);
    }

    // Current item music bed going to start soon?
    if (playback_events.intmusic_bed_starts_ms < intnext_playback_safety_margin_ms) {
      // Yes. Queue a background music bed "start"
      // - But only if it wasn't previously queued!
      if (!run_data.current_item.music_bed.already_handled.blnstart) {
        queue_event(events, "current_music_bed_start", playback_events.intmusic_bed_starts_ms);
      }
    }

    // Current item music bed going to end soon?
    if (playback_events.intmusic_bed_ends_ms < intnext_playback_safety_margin_ms) {
      // Yes. Queue a background music bed "end"
      // - But only if it wasn't previously queued!
      if (!run_data.current_item.music_bed.already_handled.blnstop) {
        queue_event(events, "current_music_bed_stop", playback_events.intmusic_bed_ends_ms);
      }
    }

    // Current item going to be interrupted by a promo sometime soon?
    // Don't do this if the next item is already loaded!
    if (playback_events.intpromo_interrupt_ms < intnext_playback_safety_margin_ms) {
      // Yes. Queue a music -> promo transition.

      // Queue a fade-out for the current item:
      queue_volslide(events, "current", 100, 0, playback_events.intpromo_interrupt_ms, config.intcrossfade_length_ms);
      // Queue: setup an xmms for the next item.
      queue_event(events, "setup_next", playback_events.intpromo_interrupt_ms + config.intcrossfade_length_ms + 1);
      // Queue: next item play
      queue_event(events, "next_play", playback_events.intpromo_interrupt_ms + config.intcrossfade_length_ms + 2);
      // Queue: next item -> current item.
      queue_event(events, "next_becomes_current", playback_events.intpromo_interrupt_ms + config.intcrossfade_length_ms + 3);
      intnext_becomes_current_ms = playback_events.intpromo_interrupt_ms + config.intcrossfade_length_ms + 3;
    }

    // Now sort the queue.
    sort (events.begin(), events.end(), transition_event_less_than);

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
        log_warning("Detected: System clock moved back " + itostr(tvdiff.tv_sec) +" seconds and " + itostr(tvdiff.tv_usec) + " usecs. Optimistically adjusting playback timing back by this amount.");
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
          if (event_split.count() < 1 || event_split.count() > 2) my_throw("Logic error");

          // Fetch main command, and an arg if present.
          strcmd = event_split[0];
          if (event_split.count() == 2) {
            strarg = event_split[1];
          }
        }

        // Handle the various commands:
        if (strcmd == "setup_next") {
          // Here we fetch an available XMMS session, or LineIn. If we're using XMMS then
          // also load the item into XMMS, setup the volume, etc.

          // Is the next_item loaded?
          if (!run_data.next_item.blnloaded) my_throw("Logic Error!");
          // Are  there already XMMS or LineIn sessions allocated for the next item? (background or foreground)
          if (run_data.sound_usage_allocated(SU_NEXT_FG) || run_data.sound_usage_allocated(SU_NEXT_BG)) my_throw("Logic Error!");

          // Nope. So setup either XMMS or LineIn:
          // * Only run this logic if the item is not in a silence segment.
          if (run_data.next_item.cat != SCAT_SILENCE) {
            if (run_data.next_item.strmedia == "LineIn") {
              testing_throw;
              // Next item will play through linein.
              // Is LineIn already allocated to something else?
              if (run_data.linein_usage != SU_UNUSED) log_message("LineIn is already used for something, but commandeering it anyway for the next item.");
              run_data.linein_usage = SU_NEXT_FG;
            }
            else {
              // Next item will play through XMMS.
              // Does the item exist?
              if (!file_exists(run_data.next_item.strmedia)) my_throw("File not found! " + run_data.next_item.strmedia);

              // - Fetch a free XMMS session
              int intsession = run_data.get_free_xmms_session(); // Will throw an exception if there aren't any free.
              // - Reserve the session for next item/foreground:
              run_data.set_xmms_usage(intsession, SU_NEXT_FG);
              // - Populate the XMMS session:
              run_data.xmms[intsession].playlist_clear();
              run_data.xmms[intsession].playlist_add_url(run_data.next_item.strmedia);

              // - Set XMMS volume to the next item's volume:
              run_data.xmms[intsession].setvol(get_pe_vol(run_data.next_item.strvol));

              // - Make sure that repeat is turned off.
              run_data.xmms[intsession].setrepeat(false);
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
            intvol = &intvol_next;
          }

          // - Item loaded?
          if (!item->blnloaded) my_throw("Logic Error!");

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
              testing_throw;
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
              run_data.xmms[intsession].setvol((get_pe_vol(item->strvol) * intpercent)/100);

              // Set volume of music bed also if it is active now:
              {
                int intsession = -1;
                try {
                  intsession = run_data.get_xmms_used(SU_BG);
                  testing_throw;
                } catch(...) {
                  // There is no music bed XMMS session allocated at this time. Do nothing.
                }
                if (intsession != -1) {
                  testing_throw;
                  // We have an XMMS session for the music bed.
                  // Extra check: Does the item actually have a music bed?
                  if (!item->blnmusic_bed) my_throw("Logic Error!");
                  run_data.xmms[intsession].setvol((get_pe_vol(item->music_bed.strvol) * intpercent)/100);
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
            if (!run_data.next_item.blnloaded) my_throw("Logic Error!");

            // Next item can't be linein:
            if (run_data.next_item.strmedia == "LineIn") my_throw("Don't use next_play commands for LineIn!");

            // Fetch the XMMS session:
            int intsession = run_data.get_xmms_used(SU_NEXT_FG);

            // Is XMMS already playing?
            if (run_data.xmms[intsession].playing()) my_throw("Don't use next_play when XMMS is already running!");

            // Start it playing now.
            run_data.xmms[intsession].play();

            // Log that we're playing it:
            {
              // Fetch the song length:
              // - First sleep for 1/10th of a second. XMMS's get_song_length() gives a
              // confused output if you call it too soon after play().
              sleep_ms(100);
              int intlength = run_data.xmms[intsession].get_song_length();
              char chlength[10];
              sprintf(chlength, "%d:%02d", intlength/60, intlength%60);
              log_message("Playing (xmms " + itostr(intsession) + ": " + itostr(get_pe_vol(run_data.next_item.strvol)) + "%): \"" + run_data.xmms[intsession].get_song_file_path() + "\" - \"" + run_data.xmms[intsession].get_song_title() + "\" (" + (string)chlength + ". Ends: " + format_datetime(now() + intlength, "%T") + ")");
            }

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

            // If it is music, then log the details to the database:
            if (run_data.next_item.cat == SCAT_MUSIC) {
              log_song_played(mp3tags.get_mp3_description(run_data.next_item.strmedia));

              // Also maintain a list of the most recently played media.
              // This helps to prevent repeating of songs (eg: change from music -> links -> music)
              run_data.remember_recent_music(run_data.next_item.strmedia);
            }

            // Check if there are any music bed events (for the next item) that will occur *during* the current
            // fade (ie, before we switch over completely to the next item), and queue them here.
            if (blncrossfade && run_data.next_item.blnmusic_bed) {
  testing_throw;
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
          if (blnfound) my_throw("Logic Error!");

          // Fetch an available XMMS session
          int intsession = run_data.get_free_xmms_session(); // Will throw an exception if there aren't any free
          run_data.set_xmms_usage(intsession, SU_BG);

          // Setup the session
          run_data.xmms[intsession].playlist_clear();
          run_data.xmms[intsession].playlist_add_url(pe->music_bed.strmedia);
          run_data.xmms[intsession].setvol((get_pe_vol(pe->music_bed.strvol)*intvol)/100);
          run_data.xmms[intsession].setrepeat(false);

          // Start the session
          run_data.xmms[intsession].play();
          // Log that we're playing underlying music:
          log_message("Music Bed (xmms " + itostr(intsession) + ": " + itostr(get_pe_vol(pe->music_bed.strvol)) + "%): \"" + run_data.xmms[intsession].get_song_file_path() + "\" - \"" + run_data.xmms[intsession].get_song_title() + "\"");

          // Fetch the length of the music bed according to XMMS:
          {
            int intmusic_bed_length_ms_xmms = run_data.xmms[intsession].get_song_length_ms();

            // If the music bed's length is unknown (or was listed as too long in the db), update it here:
            if (pe->music_bed.intlength_ms > intmusic_bed_length_ms_xmms) {
              pe->music_bed.intlength_ms = intmusic_bed_length_ms_xmms;
            }
          }

          // If the music bed's length is unknown,

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
          run_data.xmms[intsession].stop();
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


/*


** When starting a new item, calculate how far it will overshoot the end of the current segment by.
** Apply a lot of things here.. see current_data.
* Remember to add on current_item.overshoot value to segment lag. and then afterwards do
* reclaim if this is a music segment...

      // If this is a music segment, and we're behind, then shorten the length of the music segment
    // to reclaim lost time
    testing_throw;
    if (run_data.current_segment.cat.cat == SCAT_MUSIC && inttotal_segment_push_back > 0) {
      testing_throw;
      // Reduce music segments down to a minimum of 60 seconds:
      if (run_data.current_segment.intlength > 60) {
        int intreclaim =

      }
    }


*/

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
