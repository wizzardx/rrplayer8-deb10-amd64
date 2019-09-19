// This file contains the implementation for the player class's
// "get_next_item" functions. I put these in a separate file to
// speed up compilation.

#include "player.h"
#include "common/maths.h"
#include "common/my_string.h"
#include "common/rr_date.h"

#include "common/testing.h"

void player::get_next_item(programming_element & item, const int intstarts_ms) {
  // intstarts_ms - how long from now (in ms) the item will be played.
  // Fetch the next item to be played, after the current one. Checks for announcements (promos) to play
  // format clock details, music, linein, etc, etc. Also sets a flag which determines if a crossfade
  // must take place when transitioning to the next item. When we reach the store closing hour, the next
  // item is automatically the "Silence" category (overriding anything else that might want to play now)
  // Also here must come logic for when a) repeat runs out before the segment end, and b) when some time of
  // the next segement is used up by accident (push slots forwards by up to 6 mins, reclaim space by using up music time).
  log_debug("Fetching the next item");

  // Check if the item is already loaded:
  if (item.blnloaded) my_throw("Item is already loaded! Can't load it again! Reset it first!");

  // Reset "next_item"
  item.reset();

  // Update the store status, check if the store is currently open or closed:
  load_store_status();

  // Stop playback if we are within store hours:
  if (!item.blnloaded && !store_status.blnopen) {
    log_debug("Store is closed, next item is silence");
    // Store is closed. Next item is silence.
    item.cat = SCAT_SILENCE;
    item.blnloaded = true;
    // Reset our "segments delayed by" factor:
    run_data.intsegment_delay=0;

    // Also clear any promo batch we were busy with. We don't want to
    // resume this batch when the store opens again.
    run_data.waiting_promos.clear();
  }

  // Check for Format Clock segment changes
  log_debug(" - Checking for Format Clock segment change");
  get_next_item_check_fc_seg_change(intstarts_ms);

  // Return the next promo if there is one waiting in the database (or in a recently-retrieved batch):
  if (!item.blnloaded) {
    // Inside store hours. Any promos?
    log_debug(" - Don't have the next item yet, checking for a promo");
    get_next_item_promo(item, intstarts_ms, false);
  }

  if (!item.blnloaded) {
    // No promos. Use Format Clocks to determine the next item.
    log_debug(" - Don't have the next item yet, checking for a format clock item");
    get_next_item_format_clock(item, intstarts_ms);
  }

  // Check if we found something to play:
  if (!item.blnloaded) my_throw("Could not find the next item!");

  log_debug("Found item: " + item.strmedia);
}

// Functions called by get_next_item():
void player::get_next_item_promo(programming_element & item, const int intstarts_ms, const bool blnwould_interrupt_song) {
  log_debug("Checking if there is a promo (regular Radio Retail announcement) to play...");

  // Check for Format Clock segment changes:
  bool blnfc_seg_changed = false;
  {
    static long lnglast_fc_seg = -1;
    if (run_data.current_segment->blnloaded &&
        run_data.current_segment->lngfc_seg != lnglast_fc_seg) {
      log_debug(" - Detected: Format Clock segment just changed");
      lnglast_fc_seg = run_data.current_segment->lngfc_seg;
      blnfc_seg_changed = true;
    }
  }

  // Reset our list of waiting promos if the format clock segment changed and
  // the new segment doesn't allow promos (if there were "forced time" adverts
  // then those will be queried for again.
  if (blnfc_seg_changed &&
      !run_data.waiting_promos.empty() &&
      !run_data.current_segment->blnpromos) {
    log_message("Format Clock segment changed, and the new segment does not allow promos (except for 'forced time' promos)");
    log_message("Clearing any non-'forced time' promos from our list");
    programming_element_list::iterator pel_iter = run_data.waiting_promos.begin();
    while (pel_iter != run_data.waiting_promos.end()) {
      if (!pel_iter->promo.blnforced_time) {
        log_message(" - Removing " + pel_iter->strmedia + " from the list");
        pel_iter = run_data.waiting_promos.erase(pel_iter);
      }
      else pel_iter++;
    }
  }

  // Do we have anly queued promos waiting to play?
  log_debug("Do we have any queued promos waiting to play?");
  if (run_data.waiting_promos.empty()) {
    // No promos waiting to be returned. So query the database (if it's ok at
    // this time)
    log_debug(" - No. Will check if we can query for promos to add to the queue.");

    // Timing variables:
    static datetime dtmlast_now = datetime_error; // Used to check for system clock changes
    static datetime dtmlast_run = datetime_error; // The last time the function's main logic ran.

    // Check if the system clock was set back in time since the last time:
    datetime dtmnow = now();
    if (dtmnow < dtmlast_now) {
      log_message("System clock change detected, recallibrating function timing...");
      dtmlast_run = datetime_error;
      run_data.dtmlast_promo_batch_item_played = datetime_error;
    }

    // Don't query the database for adverts too regularly. Require that either at
    // least 30 seconds have elapsed from the last time, or that the Format Clock
    // segment has changed
    bool blnallow_query = false;

    log_debug("Can we query the database for adverts now?");

    if (!blnallow_query) {
      if (dtmlast_run == datetime_error || (dtmnow/30 != dtmlast_run/30)) {
        log_debug(" - Yes. Enough time has elapsed since the last query");
        blnallow_query = true;
      }
      else log_debug(" - (Not enough time has elapsed since the last query)");
    }
    if (!blnallow_query) {
      if (blnfc_seg_changed) {
        log_debug(" - Yes. The Format Clock segment has just changed");
        blnallow_query = true;
      }
    }

    if (!blnallow_query) {
      log_debug(" - No. See above for more info.");
      return;
    }

    // Now remember the last time we ran:
    dtmlast_run = dtmnow;

    // Now check that the minimum time has passed since the last announcement batch

    // Check if any ads can play now (ie, in a regular batch), or only adverts which have a specific "forced" time
    // (these could end up in "batches" also, but they take precedence over the artificial forced waits between
    // batches
    log_debug("Are regular advert batches allowed now?");
    bool blnAdBatchesAllowedNow = true; // Assume they are allowed for the moment

    // Define a macro to make our logic below simpler:
    #define CHECK(COND, PASS_MSG, FAIL_MSG) { \
      if (blnAdBatchesAllowedNow) { \
        if (COND) { \
          log_debug(" - (" PASS_MSG ")"); \
        } \
        else { \
          log_debug(" - No. " FAIL_MSG); \
          blnAdBatchesAllowedNow = false; \
        } \
      } \
    }

    CHECK(run_data.current_segment->blnloaded,
      "Current segment is loaded",
      "Current segment is not loaded");
    CHECK(run_data.current_segment->blnpromos,
      "Current segment allows promos",
      "Current segment does not allow promos");
    {
      datetime dtmwaiting_until = run_data.dtmlast_promo_batch_item_played + 60 * config.intmin_mins_between_batches;
      CHECK((dtmnow >= dtmwaiting_until),
        "Enough time has elapsed since the last advert batch",
        "Not enough time has elapsed since the last advert batch. Waiting until " + format_datetime(dtmwaiting_until, "%T"));
    }
    if (blnAdBatchesAllowedNow) {
      if (blnwould_interrupt_song) {
        if (run_data.current_item.strmedia == "LineIn") {
          log_debug(" - (Promo would interrupt LineIn, this is always allowed)");
        }
        else if (config.blnpromos_wait_for_song_end) {
          log_debug(" - No. Promo would interrupt a song and this is not allowed (tbldefs.blnAdvertsWaitForSongEnd=true)");
          blnAdBatchesAllowedNow = false;
        }
        else {
          log_debug(" - (Promo would interrupt a song, but this is allowed (tbldefs.blnAdvertsWaitForSongEnd=false))");
        }
      }
      else log_debug(" - (Promo wouldn't interrupt a song)");
    }

    if (blnAdBatchesAllowedNow) {
      log_debug(" - Advert batches are allowed now. Check above for more info");
    }
    else {
      log_debug(" - Advert batches are not allowed now.");
      log_debug("   However, promos with 'forced' times will still be played.");
      log_debug("   Check above for more info.");
    }

    // Remove the macro:
    #undef CHECK

    // Correct announcements that were previously interrupted during playback...
    correct_waiting_promos();

    // Calculate the date and time that the caller wants ads from:
    datetime dtmplayback = now() + intstarts_ms/1000;
    datetime dtmplayback_date = get_datetime_date(dtmplayback);
    datetime dtmplayback_time = get_datetime_time(dtmplayback);

    // Get the range of times to query for. The exact logic depends on whether
    //  Format Clocks are enabled or not.
    string psql_query_from = "";
    string psql_query_until = "";
    {
      datetime dtmquery_from = datetime_error;
      datetime dtmquery_until = datetime_error;

      // When do we start missing ads before? (we query for ads after this time):
      datetime dtmmiss_ads_before = get_miss_promos_before_time();

      if (config.blnformat_clocks_enabled) {
        // When Format Clocks are enabled we query for ads between X and Y, where:
        //   - X = the start of the current hour, or [current time minus minutes
        //         after which we miss adverts], whichever is earliest
        //   - Y = the current time (if we're not in an FC seg), otherwise the end
        //         of the current segment, or 10 minutes into the future, or the
        //         end of the hour in which the segment started, whichever comes
        //         earliest.
        datetime dtmhour_start = (dtmplayback_time / (60*60)) * (60*60);
        dtmquery_from = MIN(dtmhour_start, dtmmiss_ads_before);
        if (run_data.current_segment->blnloaded) {
          // Store the various times in variables, so we can get the earliest time:
          datetime dtmseg_start = get_datetime_time(run_data.current_segment->dtmstart);
          // - End of the current segment:
          datetime dtmseg_end = clamp_time(dtmseg_start + run_data.current_segment->intlength - 1);
          // - 10 minutes into the future:
          datetime dtmten_mins = clamp_time(dtmplayback_time+10*60);
          // - End of the hour in which the segment started:
          datetime dtmseg_start_hour_end = clamp_time(((dtmseg_start / (60*60)) * (60*60)) + (60*60)-1);

          // Now get the time we query up until:
          dtmquery_until = MIN(MIN(dtmseg_end, dtmten_mins), dtmseg_start_hour_end);
        }
        else {
          dtmquery_until = dtmplayback_time;
        }
      }
      else {
        // When Format Clocks are disabled we query for ads between X and Y, where:
        //   - X = the current time minus [the number of minutes after which we miss
        //         adverts]
        //   - Y = the current time
        dtmquery_from = dtmmiss_ads_before;
        dtmquery_until = dtmplayback_time;
      }
      psql_query_from = time_to_psql(dtmquery_from);
      psql_query_until = time_to_psql(dtmquery_until);
    }

    // This query is changed in version 6.14 - ad batches are restricted to certain intervals, but adverts forced to
    // play at specific times ignore these intervals and will play as close to their playback times as possible

    // Version 6.14.3 - If necessary, check the the related tblprerec_item's lifespan...
    string strSQL = "SELECT"
                  " tblSchedule_TZ_Slot.lngTZ_Slot, tblSched.strFilename, lower(tblSched.strProductCat) AS strProductCat,"
                  " tblSched.strPlayAtPercent, tblSchedule_TZ_Slot.dtmDay, tblSlot_Assign.dtmStart, tblSlot_Assign.dtmEnd,"
                  " tblSched.strPriorityOriginal, tblSched.strPriorityConverted, tblSchedule_TZ_Slot.bitScheduled,"
                  " tblSchedule_TZ_Slot.bitPlayed, tblSchedule_TZ_Slot.dtmForcePlayAt,"
                  " lower(tblsched.strAnnCode) AS strAnnCode,"
                  " lower(tblsched.strprerec_mediaref) AS strprerec_mediaref," // 6.14.3 - The 2 fields related to PAYB
                  " tblSched.bitcheck_prerec_lifespan "
                  "FROM tblschedule_tz_slot "
                  "INNER JOIN tblSlot_Assign USING (lngAssign) "
                  "INNER JOIN tblSched ON tblSchedule_TZ_Slot.lngSched = tblSched.lngSchedule "
                  "WHERE"
                  "  tblSchedule_TZ_Slot.dtmDay = date '" + format_datetime(dtmplayback_date, "%F")  + "'" + " AND "
                  "  tblSchedule_TZ_Slot.bitScheduled = " + itostr(ADVERT_SNS_LOADED) + " AND "
                  "("
                      "("
                        "(tblSchedule_TZ_Slot.dtmForcePlayAt >= " + psql_query_from + ") AND "
                        "(tblSchedule_TZ_Slot.dtmForcePlayAt <= " + psql_query_until + ")"
                      ")";

    // The query above only asks for "forced" playback times. Now if they are allowed now, then also include an
    // "OR" which will include the regular, un-forced times.
    if (blnAdBatchesAllowedNow) {
      strSQL += " OR "
                    "("
                      "(tblSchedule_TZ_Slot.dtmForcePlayAt IS NULL) AND"
                      "(tblSlot_Assign.dtmStart >= " + psql_query_from + ") AND "
                      "(tblSlot_Assign.dtmStart <= " + psql_query_until + ")"
                    ")";
    }

    // Now close the "WHERE" block and append the rest of the query.
    strSQL += ") ORDER BY tblSched.strPriorityConverted, tblSlot_Assign.dtmStart, tblSchedule_TZ_Slot.lngTZ_Slot";

    // - Player v6.15 - Now the announcement priority code (CA=1,SP=2,AD=3) actually has an effect on the announcement
    // playback priority (ORDER BY tblSched.strPriorityConverted)
    log_debug("Querying database for adverts. SQL: " + strSQL);
    ap_pg_result RS = db.exec(strSQL);
    log_debug("Returned rows: " + itostr(RS->size()));

    // Get a list of all the announcements fetched from the db, and re-order them
    // appropriately
    TWaitingAnnouncements reordered_db_announcements;
    {
      // Get all the announcements from the database
      log_debug("Fetching adverts from database");
      TWaitingAnnouncements db_announcements;
      while (*RS) {
        TWaitingAnnounce Announce;
        Announce.dbPos = strtoi(RS->field("lngTZ_Slot", "-1"));
        Announce.strFileName = lcase(RS->field("strFileName", ""));
        Announce.strProductCat = RS->field("strProductCat", "");
        Announce.dtmTime = parse_psql_time(RS->field("dtmForcePlayAt", RS->field("dtmStart", "").c_str()));
        Announce.blnForcedTime = !RS->field_is_null("dtmForcePlayAt");
        Announce.strPriority = RS->field("strPriorityOriginal", "");

        // Get strPlayAtPercent
        {
          // Fetch from the database:
          string strPlayAtPercent = RS->field("strPlayAtPercent", "");
          // Parse it further:
          if (isint(strPlayAtPercent)) {
            // Clip the value from 0 to 100
            if (strtoi(strPlayAtPercent) > 100) {
              strPlayAtPercent="100";
            }
            else if (strtoi(strPlayAtPercent) < 0) {
              strPlayAtPercent = "0";
            }
          }
          else {
            // Convert playback volume percentage to upper case
            strPlayAtPercent = ucase(strPlayAtPercent);
            if (strPlayAtPercent != "MUS" && strPlayAtPercent != "ADV") {
              strPlayAtPercent = "100";
            }
          }
          Announce.strPlayAtPercent = strPlayAtPercent;
        }

        Announce.strAnnCode = RS->field("strAnnCode", "");

        // Get the path of the announcement (ie the directory):
        {
          string strFilePath = "";
          string strFilePrefix = lcase(substr(Announce.strFileName, 0, 2));
          if (strFilePrefix == "ca") {
            strFilePath = config.dirs.strannouncements; // Announcements
          }
          else if (strFilePrefix == "sp") {
            strFilePath = config.dirs.strspecials; // Specials
          } else if (strFilePrefix == "ad") {
            strFilePath = config.dirs.stradverts; // Adverts
          }
          else {
            log_error ("Advert filename " + Announce.strFileName +
              " has an unknown prefix " + strFilePrefix);
            strFilePath = config.dirs.strmp3; // Default to the music folder
          }
          Announce.strPath = strFilePath;
        }

        // Get PAYB details:
        Announce.strPrerecMediaRef = lcase(RS->field("strprerec_mediaref", ""));
        Announce.blnCheckPrerecLifespan = (RS->field("bitcheck_prerec_lifespan", "0") == "1");

        // We now have all the the info for the advert

        // Skip the ad if it is a "forced time" ad in the future:
        if (Announce.blnForcedTime && Announce.dtmTime > dtmplayback_time) {
          log_debug(" - Not including advert " + Announce.strFileName +
            " in list, it has a forced time (" +
            format_datetime(Announce.dtmTime, "%T") + ") and is in the future");
        }
        else {
          // No problem, so add it to the list of adverts loaded from the database:
          db_announcements.push_back(Announce);
        }
        (*RS)++; // Move to the next record
      }

      // Now re-order the adverts:
      {
        log_debug("Re-ordering adverts");
        TWaitingAnnouncements::iterator iter;
        // - First "forced playback time" adverts
        //   (we already filtered out ones in the future)
        iter = db_announcements.begin();
        while (iter != db_announcements.end()) {
          if (iter->blnForcedTime) {
            log_debug(" - Moving advert " + iter->strFileName +
              " with forced time (" + format_datetime(iter->dtmTime, "%T") +
              ") to start of re-ordered advert list");
            reordered_db_announcements.push_back(*iter);
            iter = db_announcements.erase(iter);
          }
          else iter++;
        }

        // A macro to simplify our logic for moving items which play between
        // 2 times from db_announcements to reordered_db_announcements
        #define MOVE_ADS_BETWEEN(FROM, TO, DESC) \
          iter = db_announcements.begin(); \
          while (iter != db_announcements.end()) { \
            if (iter->dtmTime >= (FROM) && \
                iter->dtmTime <= (TO)) { \
              log_debug(" - Advert " + iter->strFileName + " (at " + \
              format_datetime(iter->dtmTime, "%T") + ") is " + DESC + \
              ". Appending to re-ordered list"); \
              reordered_db_announcements.push_back(*iter); \
              iter = db_announcements.erase(iter); \
            } \
            else iter++; \
          }

        // - Then adverts in this hour:
        datetime dtmhour_start = (dtmplayback_time / (60*60)) * (60*60);
        datetime dtmhour_end = dtmhour_start + (60*60) - 1;

        // - Related to the current segment (if it is loaded):
        if (run_data.current_segment->blnloaded) {
          datetime dtmseg_start = get_datetime_time(run_data.current_segment->scheduled.dtmstart);
          datetime dtmseg_end   = get_datetime_time(run_data.current_segment->scheduled.dtmend);

          // - Adverts in the current format clock segment
          MOVE_ADS_BETWEEN(dtmseg_start, dtmseg_end, "in this hour and inside the current Format Clock segment");
          // - Adverts after the current format clock segment
          MOVE_ADS_BETWEEN(dtmseg_end+1, dtmhour_end, "in this hour and after the current Format Clock segment");
          // - Adverts before the current format clock segment
          MOVE_ADS_BETWEEN(dtmhour_start, dtmseg_start-1, "in this hour and before the current Format Clock segment");
        }
        // All other adverts in this hour (eg, if the segment wasn't loaded)
        MOVE_ADS_BETWEEN(dtmhour_start, dtmhour_end, "in this hour (no Format Clock segments loaded?)");

        // - Lastly, all other adverts (outside the current hour)
        MOVE_ADS_BETWEEN(DATETIME_MIN, DATETIME_MAX, "outside the current hour");

        // Make sure that the list of adverts read from the database is now empty:
        if (!db_announcements.empty()) LOGIC_ERROR;

        // Now undefine our macro:
        #undef MOVE_ADS_BETWEEN
      }
    }

    // Process the re-ordered list of items from the database, and build the list
    // of ads to actually be played in this batch:
    TWaitingAnnouncements AnnounceList; // Ads to be played in the next batch
    TWaitingAnnouncements AnnounceMissed_SameVoice; // These are the announcements missed because the
                                                    // previous announcement had the same announcer

    TWaitingAnnouncements::iterator reordered_iter = reordered_db_announcements.begin();
    while (reordered_iter != reordered_db_announcements.end()
          && AnnounceList.size() < (unsigned) config.intmax_promos_per_batch) {

      // Skip items with problems:
      bool blnSkipItem = false; // Set to true if the announcement is to be skipped

      // - Skip non-existant files:
      {
        string strActualFileName = ""; // This is the filename of a matching file
                                      // (matching meaning there is a
                                      // case-non-sensitive match of filenames
        if (!file_existsi(reordered_iter->strPath, reordered_iter->strFileName, strActualFileName)) {
          log_error("Could not find announcement MP3: " + reordered_iter->strPath + reordered_iter->strFileName);
          blnSkipItem = true;
        }
      }

      // v6.14.3 - PAYB stuff:
      if (reordered_iter->blnCheckPrerecLifespan) {
        if (reordered_iter->strPrerecMediaRef == "") {
          // The bit for checking the lifespan is set, but the media reference field is empty
          log_error("tblsched.bitcheck_prerec_lifespan set to 1, but tblsched.strprerec_mediaref is empty!");
          blnSkipItem = true;
        }
        else {
          // strprerec_mediaref and bitcheck_prerec_lifespan are set. Retrieve lifespan and global expiry date info from
          // the related tblprerec_item record
          string psql_PrerecMediaRef = psql_str(lcase(reordered_iter->strPrerecMediaRef));
          strSQL = "SELECT intglobalexp, intlifespan FROM tblprerec_item WHERE lower(strmediaref) = " + psql_PrerecMediaRef;

          ap_pg_result rsPrerecItem = db.exec(strSQL);
          // Check the results of the query.
          if (rsPrerecItem->size() != 1) {
            // We expected to find 1 matching record, but a different number was found
            log_error(itostr(rsPrerecItem->size()) + " prerecorded items match media reference " + reordered_iter->strPrerecMediaRef + ". Cannot play " + reordered_iter->strFileName);
            blnSkipItem = true;
          }
          else {
            // We found 1 matching record, and found it. Now check the current date against the listed global expiry date, and
            // the current lifespan, of the prerecorded item.
            int intglobalexp = strtoi(rsPrerecItem->field("intglobalexp", "-1"));
            int intlifespan = strtoi(rsPrerecItem->field("intlifespan", "-1"));

            // Get today's rr date, as an integer.
            int inttoday_rrdate = get_rrdateint(date());

            // Check the global expiry date
            if ((intglobalexp != -1) && (intglobalexp < inttoday_rrdate)) {
              // Global expiry date has elapsed!
              log_error("Advert skipped because because it's global expiry date has passed: " + reordered_iter->strFileName);
              blnSkipItem = true;
            }
            // Check the lifespan.
            else if ((intlifespan != -1) && (intlifespan < inttoday_rrdate)) {
              // Lifespan has elapsed!
              log_error("Advert skipped because the period it was purchased for has expired: " + reordered_iter->strFileName);
              blnSkipItem = true;
            }
          }
        }
      }

      bool blnAnnouncerClash = false; // Set to true if the announcement was skipped because the
                                      // announcer is the same as the previous one listed to be played.

      // Are there any announcements already in the queue?
      if (!blnSkipItem && AnnounceList.size() > 0) {
        // Two checks: First, the same announcement cannot play twice in an announcement batch
        //             Secondly, the same announcer cannot play twice in succession

        // . Check 1: Do not allow the same file, catagory or playback instance ID to play twice in the same announcement batch.
        TWaitingAnnouncements::const_iterator item = AnnounceList.begin();
        while (!blnSkipItem && item!=AnnounceList.end()) {
          blnSkipItem = (item->strFileName==reordered_iter->strFileName) || (reordered_iter->strProductCat != "" && item->strProductCat == reordered_iter->strProductCat);
          ++item;
        }

        // . Check 2: Do not allow ads by the same announcer to play twice in succession:
        if (!blnSkipItem && reordered_iter->strAnnCode != "") {
          if (AnnounceList[AnnounceList.size()-1].strAnnCode == reordered_iter->strAnnCode) {
            blnSkipItem = true;
            blnAnnouncerClash = true;
          }
        }
      }

      // Now store the announcement details - we will either add it to the list of "to-play" items, or
      // the skipped (because of announcer clashes) list. If at the end of processing the announcements that
      // want to play now, we have not reached the bax maximum allowed announcements, we can try very hard
      // to find a place in the 'to-play' list where skipped announcements can be played without causing two announcements
      // by the same announcer to play in succession.
      if (!blnSkipItem || blnAnnouncerClash) {
        if (!blnSkipItem) {
          // This announcement will be played, queue it in the list of announcements to be played
          AnnounceList.push_back(*reordered_iter);
        }
        else {
          // This announcement possibly will not play, because the announcer was the same as the last
          // announcer in the "to-play" queue. We will try later to add it to the list anyway (if the maximum
          // allowed number of announcements per batch has not yet been reached)
          AnnounceMissed_SameVoice.push_back(*reordered_iter);
        }
      }
      reordered_iter++; // Move to the next record...
    }

    // We've reached either the end of the recordset, or the limit for number of announcements to play in a single batch.

    // If we have not reached the maximum number of allowed announcements, but there were announcements skipped
    // because their announcer was the same as the previously-queued announcer, then...
    if (AnnounceList.size() < (unsigned) config.intmax_promos_per_batch &&
      AnnounceMissed_SameVoice.size() > 0) {

      // There are missed announcements (because of the same voice), but there is still space for announcements.. here comes
      // the fun algorithm for finding a home in the "to-play" list for these voices!

      // Each time that we try to fit new announcements into the "to-play" list, and are successful, the "to-play" list length will
      // grow. If the list grows, there is a chance that we can do another pass and insert more announcements with
      // clashing codes. If the announcement list size does not grow on one pass, then there is no use in trying further passes
      long lngPrevAnnListSize = -1; // Always try at least once...
      while (lngPrevAnnListSize < (signed)AnnounceList.size()) {
        lngPrevAnnListSize = AnnounceList.size(); // Now remember the current announcement list size. If this
                                                  // length increases, then we will run this loop again.. etc, etc..
        // Try to find a spot for each "same-voice" missed announcement, but only while there is space left in this
        // announcement batch...
        TWaitingAnnouncements::iterator item = AnnounceMissed_SameVoice.begin();
        while (item!=AnnounceMissed_SameVoice.end() &&
              AnnounceList.size() < (unsigned) config.intmax_promos_per_batch) {
          // Temporarily append the skipped announcement to the end of the "to-play" list
          //    . The item will stay in the queue if a valid playlist order is found, otherwise it will be removed later..

          AnnounceList.push_back(*item);
          // -- remember the DB ID of the announcement in case we mess up and don't get the permutations correct...
          unsigned long test_dbPos = item->dbPos;

          // - Calculate the number of permutations possible with the new "to-play" length
          long lngPermutations = calc_permutations(AnnounceList.size());

          // Start the permutation-generation... loop
          bool blnValidSequenceFound = false; // set to true if a valid order for the ads is found...
          long lngPermutationNum = 1; // The number of the current permutiation out of the total possible.

          // Init variables used for generating permutations...
          long lngTransposePos = 0; // We swap two elements, the one at this pos, and the one just after.
          int intTransposeDir = 1; // After doing a swap, the next permuation will be generated by swapping two adjacent elements
                                  // - eg after swapping element 1 and 2, we swap element 2 and 3. When we reach
                                  //   the end of the list, we start moving backwards again - eg, swap 9 and 10, then swap 8 and 9
                                  // - intTransposDir controls how lngTransposePos progresses.

          while (lngPermutationNum <= lngPermutations && !blnValidSequenceFound) {
            // Firstly, is this "to-play" list valid? Check that the same announcer code does not occur twice in succession.
            bool blnTestFailed=false;
            string strPrevAnnCode = AnnounceList[0].strAnnCode;

            for (long lngAnnTest=1;lngAnnTest < (signed)AnnounceList.size() && !blnTestFailed;++lngAnnTest) {
              string strAnnCode = AnnounceList[lngAnnTest].strAnnCode;
              if (strAnnCode != "" && strAnnCode==strPrevAnnCode) {
                // Same announcement code found - test failed
                blnTestFailed = true;
              }
              else {
                // Different announcer code - update the "prev" announcer code
                strPrevAnnCode = strAnnCode;
              }
            }
            blnValidSequenceFound=!blnTestFailed;

            // Was a valid announcement order found?
            if (!blnValidSequenceFound) {
              //  If it isn't, then generate the next "transposition order" permutation, by exchanging two adjacent elements
              // of the "to-play" announcement list. Thanks go to the Canadian web-page
              // "http://www.schoolnet.ca/vp-pv/amof/e_permI.htm" where some concepts behind generating permutations are
              // explained.

              // Swap two adjacent elements...
              TWaitingAnnounce TempAnn = AnnounceList[lngTransposePos];
              AnnounceList[lngTransposePos] = AnnounceList[lngTransposePos+1];
              AnnounceList[lngTransposePos+1] = TempAnn;

              // Are there more than 2 elements?
              if (AnnounceList.size() > 2) {
                // Check the current swap pos - have we reached the end of the current direction? (depends on the
                // current direction)
                if ((intTransposeDir==1 && (lngTransposePos>=(signed)AnnounceList.size()-2)) ||
                    (intTransposeDir==-1 && (lngTransposePos<=0))) {
                  // Yes - Change the progress direction..
                  intTransposeDir *= -1;
                }
                // Now progress the current swap position.
                lngTransposePos += intTransposeDir;
              }
            }
            // Now we go to the next attempt to find a valid announcement order...
            ++lngPermutationNum;
          }

          bool blnEraseFromMissedList = false; // This variable determines if the next missed item
                                              // to be checked is determined either by 1)
                                              // erasing from the list (which gives you the next item)
                                              // or 2) - incrementing the iterator to the next item.
          if (blnValidSequenceFound)  {
            // We found a valid playbackorder, and the "to-play" announcement list now
            // contains the announcement that would have otherwise been missed.
            // - so now we remove the announcement from the "missed" list.
            blnEraseFromMissedList = true;
          }
          else {
            // A valid sequence was not found...
            //If no valid permutations were found, then remove the temporarily-added announcement from the end
            // of the"to-play" queue. Check that we have the correct element by comparing the DB id with the one we retrieved
            // earlier...

            // If we retrieved the incorrect DB id then log an error message and scan the the list for the correct element to remove
            // (it was only temporarily added), and remove it from there...
            if (AnnounceList[AnnounceList.size()-1].dbPos !=  test_dbPos) {
              log_error("Error in the permutation generation code! After all the permutations, the last element should be the same as it was at the beginning!");
              // The code outside this block is buggy - but attempt to correct the "listed to play" playlist anyway.

              TWaitingAnnouncements::iterator remove_item = AnnounceList.begin();
              while (remove_item!=AnnounceList.end()) {
                if (remove_item->dbPos==test_dbPos) {
                  // We've found a matching item to delete - remove it and get the item after...
                  remove_item==AnnounceList.erase(remove_item);
                  log_error("Permutation code error was corrected.. but please check the code anyway..");
                }
                else {
                  // This item isn't one to delete.. go to the next one.
                  ++remove_item;
                }
              }
            }
            else {
              // The correct DB id is at the end of the list - so just erase the last item...
              AnnounceList.erase(AnnounceList.end());
            }
          }

          if (blnEraseFromMissedList) {
            // Erase an item from the missed list, because a place in the announcement order was found.
            // for it.
            item = AnnounceMissed_SameVoice.erase(item);
          }
          else {
            // A place in the announcement list was not found, just jump normally to the next
            // item we will attempt to insert....
            item++;
          }
        }
      }
    }

    // Now, we have the final list of announcements to be played. Mark these announcements in the database
    // as "listed to be played", and log a message in the player log...
    TWaitingAnnouncements::const_iterator announce_item = AnnounceList.begin();
    while (announce_item!=AnnounceList.end()) {
      // Log a message to say that this announcement is enqued
      string strTime = format_datetime(announce_item->dtmTime, "%T");
      string strDate = format_datetime(date(), "%F");
      log_message("Announcement to be played: " + announce_item->strFileName +
                                ", priority: " + announce_item->strPriority +
                                ", catagory: \"" + announce_item->strProductCat +
                                "\", volume: " + announce_item->strPlayAtPercent +
                                "%,  time: " + strTime +
                                ", date: " + strDate +
                                ", db index: " + itostr(announce_item->dbPos));

      // Update the database also
      strSQL = "UPDATE tblSchedule_TZ_Slot SET "
                "bitScheduled = " + itostr(ADVERT_LISTED_TO_PLAY) +
                ", dtmScheduledAtDate = " + psql_date +
                ", dtmScheduledAtTime = " + psql_time +
                " WHERE lngTZ_Slot = " + itostr(announce_item->dbPos);
      db.exec(strSQL);

      // Move to the next "to play" item..
      ++announce_item;
    }

    // Now queue the list of announcements to be played.
    if (AnnounceList.size() > 0) {
      // This means that there are ads to play.

      // Now play the waiting announcements, one after another. This is a different approach:
      // previously a continuosly called function checked the status of
      // XMMS, and when an ad finished, it would check for the next ad, or resume music playback.
      while (AnnounceList.size() > 0) {
        // Fetch details about the current announcement in the queue
        TWaitingAnnounce Announce = AnnounceList[0];
        AnnounceList.pop_front(); // We have the ad details now, remove it from the queue

        string strActualFileName = "";   // This is the filename of a matching file (matching meaning there
                                        // is a case-non-sensitive match of filenames

        bool blnFileExists = true; // Set to false if the file is not found.

        // File exists?
        if (!file_existsi(Announce.strPath, Announce.strFileName, strActualFileName)) {
          testing;
          // No.
          blnFileExists = false;
          log_error("Could not find announcement MP3: " + Announce.strPath + Announce.strFileName);
          testing;
        }

        // So did we find the file?
        if (blnFileExists) {
          // Announcement MP3 found.

          // Convert old-style volume strings:
          if (Announce.strPlayAtPercent == "MUS") {
            testing;
            Announce.strPlayAtPercent = "MUSIC";
            testing;
          }
          else if (Announce.strPlayAtPercent == "ADV") {
            testing;
            Announce.strPlayAtPercent = "PROMO";
            testing;
          }

          // Queue the announcement to play...
          {
            programming_element promo;
            promo.cat       = SCAT_PROMOS;
            promo.strmedia  = Announce.strPath + Announce.strFileName;
            promo.strvol    = Announce.strPlayAtPercent;
            promo.promo.lngtz_slot = Announce.dbPos;
            promo.promo.blnforced_time = Announce.blnForcedTime;
            promo.blnloaded = true;
            run_data.waiting_promos.push_back(promo);
          }
        }
      }
    }
  } // if (run_data.waiting_promos.empty())
  else log_debug(" - Yes. Using a promo from a previously retrieved batch");

  // Are there any promos waiting to be returned?
  if (run_data.waiting_promos.empty()) {
    log_debug("No promos to play now");
  }
  else {
    // Yes: Return the promo and then leave this function
    item = run_data.waiting_promos[0];
    run_data.waiting_promos.pop_front();
    log_debug("Found a promo to play now");
  }
}

void player::get_next_item_check_fc_seg_change(const int intstarts_ms) {
  // This function is called while fetching the next item to determine if
  // the Format Clock segment has changed. This logic was separated from
  // get_next_item_format_clock() so that get_next_item_promo() (called earlier)
  // can know about Format Clock changes immediately after the FC segment
  // change.

  // Fetch the "real" time when the next item will start playing (in seconds):
  // - We take the current exact time, add the milliseconds until the start of
  //   the next item, then truncate to seconds precision. Our "segment delay
  //   factor" logic prevents any format clock time from being unaccounted for
  //   due to rounding issues.
  datetime dtmnext_starts = datetime_error;
  {
    timeval tvnow;
    gettimeofday(&tvnow, NULL);
    tvnow.tv_usec += (intstarts_ms * 1000);
    normalise_timeval(tvnow);
    dtmnext_starts = tvnow.tv_sec;
  }

  // Work out the current delays.

  // Do we currently have a segment loaded?
  if (run_data.current_segment->blnloaded) {
    // We now have the time when the next item will start.
    // Work out if the current item will play past the end of the current segment.
    // This logic works by assuming that the current item ends on the second just before
    // the next item starts. Based on that assumption, does the current item end after
    // the current segment ends? If so, by how many seconds?
    datetime dtmseg_end = run_data.current_segment->dtmstart + run_data.current_segment->intlength - 1;

    // Calculate the difference between the current item end, and the next segment start:
    int intdiff = dtmnext_starts - dtmseg_end - 1; // Take out that extra second here.

    if (intdiff > 0) {
      // Yes: Increase the segment delay factor
      int intnew_segment_delay = run_data.intsegment_delay + intdiff;
      if (intnew_segment_delay > intmax_segment_push_back) {
        log_warning("I have to drop " + itostr(intnew_segment_delay - intmax_segment_push_back) + "s of segment playback time! I've reached my segment 'delay' limit of " + itostr(intmax_segment_push_back) + "s");
        intnew_segment_delay = intmax_segment_push_back;
      }
      log_line("Current item is going to end " + itostr(intdiff) + "s after the current segment end. Increasing segment delay factor to " + itostr(intnew_segment_delay) + "s (+" + itostr(intnew_segment_delay - run_data.intsegment_delay) + "s)");
      run_data.intsegment_delay = intnew_segment_delay;
    }
    // Otherwise, is the item going to end 10 or less seconds before the next segment starts?
    else if (intdiff >= -10 && intdiff < 0) {
      // Predict if fetching the next item from the current segment will cause the
      // playback to revert (ie, start playing music instead of links etc)
      string strreason;
      if (run_data.current_segment->get_next_item_will_revert(strreason)) {
        // Warn about this. We don't currently support skipping ahead to
        // the next segment.
        log_warning("Going to revert, but there are only " + itostr(-intdiff) + "s remaining in this segment.");
        log_line("TODO: Something else for the next " + itostr(-intdiff) + "s?");
      }
    }
  }

  // Have we crossed into the next hour, or will we do so by the time the next
  // item starts?
  bool blnhour_change = false;
  {
    long lngnext_item_hour = dtmnext_starts / (60*60);
    static long lnglast_check_hour = lngnext_item_hour; // The hour when we last checked

    // Hour change?
    blnhour_change = lngnext_item_hour != lnglast_check_hour;
    lnglast_check_hour = lngnext_item_hour; // Now wait until the hour changes again
  }

  // Did we detect an hour change?
  if (blnhour_change) {
    // Hour change detected
    log_line("Hour change detected: Next item starts at " + format_datetime(dtmnext_starts, "%T"));

    // If necessary we cut off part of our segment delay so that we will start
    // fetching items from the start of the next hour instead of the previous
    // hour:
    // - Work out the maximum allowed segment delay at this point:
    int intmax_seg_delay = dtmnext_starts % (60*60); // # of seconds into the hour of the next item

    // - Does our current segment delay need to be truncated?
    if (run_data.intsegment_delay > intmax_seg_delay) {
      // Yes. Do so
      // Don't bother logging warnings if format clocks are disabled:
      if (config.blnformat_clocks_enabled) {
        log_warning("Segment delay needs to be shortened from " + itostr(run_data.intsegment_delay) + "s to " + itostr(intmax_seg_delay) + "s");
        log_warning("All segments that were scheduled to play between " + format_datetime(dtmnext_starts - run_data.intsegment_delay, "%T") +  " and " + format_datetime(dtmnext_starts - intmax_seg_delay - 1, "%T") + " will be missed!");
      }
      run_data.intsegment_delay = intmax_seg_delay;
    }
  }

  // And now get our "delayed" time, the time in the past at which we fetch format clock data
  datetime dtmdelayed = dtmnext_starts - run_data.intsegment_delay;

  // If we didn't just detect an hour change, then make sure that our
  // segment delay factor does not drift outside the current hour:
  if (!blnhour_change) {
    datetime dtmhour_start = (now() / (60*60)) * (60*60);
    datetime dtmhour_end   = dtmhour_start + (60*60) - 1;

    if (dtmdelayed > dtmhour_end) {
      log_warning("Segment delay has drifted past the end of the current hour. Clamping to the end of this hour.");
      dtmdelayed = dtmhour_end;
    }
    else if (dtmdelayed < dtmhour_start) {
      log_warning("Segment delay has somehow ended up before the start of the current hour (this shouldn't be possible). Clamping to the start of this hour.");
      dtmdelayed = dtmhour_start;
    }
  }

  long lngfc     = -1; // -1 means no Format Clock found...
  long lngfc_seg = -1; // -1 means no Format Clock segment found.

  // Are format clocks enabled?
  if (!config.blnformat_clocks_enabled) {
    // Format clocks are not enabled. We don't query the database
    log_line("Format Clocks are disabled. Will use a music profile instead.");
  }
  else {
    // Format clocks are enabled. Query for the Format Clock database id & segment for this time.
    // Include the current "segment delay" factor
    log_line("Fetching Format Clock and Segment scheduled for " + format_datetime(dtmdelayed, "%T"));

    // Variables used for fetching format clock segments:
    string strfc_date              = format_datetime(dtmdelayed, "%F");
    string strfc_time_with_hour    = format_datetime(dtmdelayed, "%T");
    string strfc_time_without_hour = format_datetime(dtmdelayed, "00:%M:%S");

    // Find a format clock and segment scheduled for this time:
    {
      // We need to fetch lngfc and lngfc_seg:
      // Fetch lngfc:
      string strsql = "SELECT lngfc FROM tblfc_sched INNER JOIN tblfc_sched_day USING (lngfc_sched) "
                      "WHERE (tblfc_sched.intday = " + itostr(weekday(dtmdelayed)) + " OR tblfc_sched.intday IS NULL) AND "
                      "date '" + strfc_date + "' BETWEEN COALESCE(tblfc_sched.dtmstart, '0001-01-01') AND "
                                                        "COALESCE(tblfc_sched.dtmend, '9999-12-25') AND "
                      "time '" + strfc_time_with_hour + "' BETWEEN tblfc_sched_day.dtmstart AND tblfc_sched_day.dtmend "
                      "ORDER BY tblfc_sched.lngfc_sched DESC LIMIT 1";
      ap_pg_result rs = db.exec(strsql);

      // How many results?
      if (rs->size() <= 0) { // No format clocks scheduled.
        log_warning("No Format Clocks scheduled for this hour. Will revert to the default Format Clock.");
      } else { // User scheduled 1 or more format clocks.
        // We found a user-scheduled format clock. Fetch the segment:
        lngfc = strtol(rs->field("lngfc"));
        try {
          lngfc_seg = get_fc_segment(lngfc, strfc_time_without_hour);
        }
        catch(const my_exception & e) {
          // We failed to get a segment.
          log_warning(e.get_error());
          log_warning("Will revert to the default format clock");
          lngfc = -1;
          lngfc_seg = -1;
        }
      }
    }

    // Did we find a format clock & segment to use?
    if (lngfc == -1) {
      // No. Fetch the Default format clock
      log_message("Reverting to Default Format Clock...");
      // Check if we have the setting:
      if (config.lngdefault_format_clock <= 0) {
        // Default clock is not set!
        log_warning("Default Format Clock is not set! Reverting to a music profile...");
      }
      else {
        // Default clock is set (in tbldefs). See if it exists on the system.
        string strsql = "SELECT lngfc FROM tblfc WHERE lngfc = " + ltostr(config.lngdefault_format_clock);
        ap_pg_result rs = db.exec(strsql);
        // Did we find 1 record?
        if (rs->size() != 1) {
          // Nope
          log_warning("Could not find the Default Format clock! (lngfc=" + ltostr(config.lngdefault_format_clock) +"). I will revert to a music profile.");
        }
        else {
          // We found the Format Clock record. Now find the current Format Clock segment
          lngfc = config.lngdefault_format_clock;
          try {
            lngfc_seg = get_fc_segment(lngfc, strfc_time_without_hour);
          }
          catch(const my_exception & e) {
            // We failed to get a segment.
            log_warning(e.get_error());
            log_warning("Will default to a music profile");
            lngfc = -1;
            lngfc_seg = -1;
          }
        }
      }
    }
  }

  // Has the current segment changed?
  // Or, does the system want to reload the segment data?
  // Or, has the current segment expired?

  // - 'Segment expired' means that the segment's time has run out.
  bool blnsegment_expired = dtmnext_starts > (run_data.current_segment->dtmstart + run_data.current_segment->intlength - 1);

  if (!run_data.current_segment->blnloaded ||
       lngfc_seg != run_data.current_segment->lngfc_seg ||
       run_data.blnforce_segment_reload ||
       blnsegment_expired) {

    // Reset the "force segment reload" control variable:
    run_data.blnforce_segment_reload = false;

    // Load the new segment:
    log_message("Loading Format Clock segment (id: " + itostr(lngfc_seg) + ")");

    // A -1 lngfc_seg means load the currently-scheduled music profile instead

    // Check if we need to get the next item ASAP (ie, use cached playlists if available), otherwise
    // scan directories etc:
    const bool blnasap = (intstarts_ms - now() <= 1); // Starts 1 or less seconds from now..
    run_data.current_segment->load_from_db(db, lngfc_seg, dtmdelayed, config, mp3tags, m_music_history, blnasap);

    // Log more info about the segment we just loaded:
    log_message("Loaded segment " + itostr(run_data.current_segment->intseg_no) +
      "/" + itostr(run_data.current_segment->fc.segments) +
      " (" + format_datetime(run_data.current_segment->scheduled.dtmstart, "%T") +
      " - " + format_datetime(run_data.current_segment->scheduled.dtmend, "%T") +
      ", " + itostr(run_data.current_segment->scheduled.dtmend - run_data.current_segment->scheduled.dtmstart + 1) +
      "s, " + run_data.current_segment->cat.strname + ") of Format Clock \"" +
      run_data.current_segment->fc.strname + "\" (id: " +
      ltostr(run_data.current_segment->fc.lngfc) + ")");

    // If this is a user-scheduled music segment, then remember the programming
    // elements (used later for reverting  when we run out of items)
    if (lngfc_seg != -1 && run_data.current_segment->cat.cat == SCAT_MUSIC) {
      prev_music_seg_pel = run_data.current_segment->programming_elements;
    }

    // How far into the segment did we query for?
    int intdiff = dtmdelayed - run_data.current_segment->scheduled.dtmstart;

    // Is our difference negative?
    if (intdiff < 0) LOGIC_ERROR;

    // Did we query part-way into the segment?
    if (intdiff > 0) {
      log_line("Currently " + itostr(intdiff) + "s into the new segment. Compensating...");
      // Try to add this difference to our current segment delay factor
      int intnew_segment_delay = run_data.intsegment_delay + intdiff;
      if (intnew_segment_delay > intmax_segment_push_back) {
        log_warning("I have to drop " + itostr(intnew_segment_delay - intmax_segment_push_back) + "s of segment playback time! I've reached my segment 'delay' limit of " + itostr(intmax_segment_push_back) + "s");
        intnew_segment_delay = intmax_segment_push_back;
      }
      if (run_data.intsegment_delay != intnew_segment_delay) {
        log_line("Increasing 'segment delay' factor to " + itostr(intnew_segment_delay) + "s (+" + itostr(intnew_segment_delay - run_data.intsegment_delay) + "s)");
        run_data.intsegment_delay = intnew_segment_delay;
        dtmdelayed = dtmnext_starts - run_data.intsegment_delay;
      }
    }

    // Update the new segment's "start" and "length" variables.
    run_data.current_segment->dtmstart  = dtmnext_starts;
    run_data.current_segment->intlength = run_data.current_segment->scheduled.dtmend - dtmdelayed + 1;

    // Reclaim "delayed by" time if this is a music segment. But leave at least 60 seconds.
    if (run_data.current_segment->cat.cat == SCAT_MUSIC && run_data.current_segment->intlength > 60) {
      int intnew_length = run_data.current_segment->intlength - run_data.intsegment_delay;
      if (intnew_length < 60) {
        intnew_length = 60;
      }
      int intdiff = run_data.current_segment->intlength - intnew_length;
      if (intdiff > 0) {
        // Hooray, we get to reclaim some space
        log_line("Reclaiming " + itostr(intdiff) + "s of 'segment delayed' time from the current (music) segment.");
        run_data.intsegment_delay -= intdiff;
        run_data.current_segment->intlength -= intdiff;
      }
    }

    // Now log some basic details about the new segment.
    log_line(run_data.current_segment->cat.strname + " segment will play between "
      + format_datetime(run_data.current_segment->dtmstart, "%T") + " and "
      + format_datetime(run_data.current_segment->dtmstart
      + run_data.current_segment->intlength - 1, "%T")
      + " (" + itostr(run_data.current_segment->intlength) + "s)");

    // If this is a music playlist then log it to the database:
    if (run_data.current_segment->cat.cat == SCAT_MUSIC) {
      // Log the XMMS playlist, and the system's available music later:
      run_data.blnlog_all_music_to_db = true;
    }
  }
  else {
    // Segment has not changed. Just log the current segment and end time.
    log_message("No segment change. " + run_data.current_segment->cat.strname + " segment (id: " + itostr(run_data.current_segment->lngfc_seg) + ") will end at " + format_datetime(run_data.current_segment->dtmstart + run_data.current_segment->intlength - 1, "%T"));
  }

  // Also log the current Format Clock (and whether it changed):
  {
    static long lngprev_fc = -1;
    string strmsg = "Format Clock (id: " + itostr(run_data.current_segment->fc.lngfc) + "): \"" + run_data.current_segment->fc.strname + "\"";

    if (lngprev_fc != run_data.current_segment->fc.lngfc) {
      log_message("Changed to a new " + strmsg);
      lngprev_fc = run_data.current_segment->fc.lngfc;
    }
    else {
      log_message("Still playing " + strmsg);
    }
  }
}

void player::get_next_item_format_clock(programming_element & next_item, const int intstarts_ms) {
  // Fetch the next item from the current format clock
  // NB: You should call get_next_item_check_fc_seg_change() before calling this function.
  //
  // Currently all this code does is call "get_next_item_not_recent_music()".
  // The main logic was moved to get_next_item_check_fc_seg_change()

  // From the end of the old version of this function:

  // Now fetch the next item to play, from the segment. Make sure it isn't a
  // song which was played recently:
  get_next_ok_music_item(next_item, intstarts_ms, m_music_history, mp3tags,
                         db, config, run_data);
}

void get_next_ok_music_item(

  // Returned programming element
  programming_element & next_item,

  // How long until the next item starts
  const int intstarts_ms,

  // Objects containing data required for this function. These are members
  // of the 'player' class that can no longer be reached directly since
  // this function is no longer a method.
  music_history & music_history,
  mp3_tags & mp3tags,
  pg_conn_exec & db,
  player_config & config,
  player_run_data & run_data) {

  // This function stops inappropriate songs from being played from the
  // playlist.
  //
  // It works by skipping songs which:
  //
  //  1) Have played recently
  //     (eg: segment changes from music to non-music and back to music)
  //
  // (That's all for now, more conditions to be added later)
  //

  bool blnok           = false; // Set to true when we find an item which isn't a recently-played song

  // Count the music items in the current playlist:
  int intnum_music_items = run_data.current_segment->count_items_from_catagory(SCAT_MUSIC);

  // Determine the minimum number of music items that should elapse before
  // a song can repeat
  // - intprevent_song_repeat_factor represents a percentage of the current
  //   music playlist's length.
  int intmin_songs_before_song_repeat = (intnum_music_items * intprevent_song_repeat_factor) / 100;

  // Attempt [current playlist length * 2] (or 100, whichever is higher) times to fetch
  // an item which hasn't been played recently. Using * 2 because it is possible for the
  // playlist to change during this process (eg, a music segment with repeating disabled,
  // and the system reverts to a music profile instead
  int intattempts_left = run_data.current_segment->programming_elements.size() * 2;
  if (intattempts_left < 100) intattempts_left = 100;

  int intnum_skipped_songs = 0; // Number of songs skipped by our logic

  while (!blnok && intattempts_left > 0) {
    // When was the current playlist previously updated?
    datetime dtmprev_playlist_update = run_data.current_segment->dtmpel_updated;

    // Check if we need to get the next item ASAP (ie, use cached playlists if available), otherwise
    // scan directories etc:
    const bool blnasap = (intstarts_ms < 1000); // Starts 1 or less seconds from now..

    // Fetch the next item:
    run_data.current_segment->get_next_item(next_item, db, intstarts_ms, config, mp3tags, music_history, blnasap);

    // If the segment playlist was just updated, then re-calculate the mimum
    // allowed number of songs before a song can repeat.
    // - eg: The segment was News, runs out of items to play, and reverts to
    //       default music. At the beginning of the function the minimum number
    //       of songs allowed before song repetition was 0 (ie, after the
    //       reverting, the next song could be a repetition of a very recent
    //       song (from just before the news segment). This is why we need to
    //       re-calculate the min # songs.
    if (run_data.current_segment->dtmpel_updated != dtmprev_playlist_update) {
      log_message("Playlist was updated during song repetition-prevention logic. Recallibrating.");
      // Reload the # music items & recalculate the min # songs before a song is allowed to repeat:
      intnum_music_items = run_data.current_segment->count_items_from_catagory(SCAT_MUSIC);
      intmin_songs_before_song_repeat = (intnum_music_items * intprevent_song_repeat_factor) / 100;
    }

    // Item is automatically ok if it is non-music
    // (eg: we music segment ran out of items and reverted to sweepers)
    if (!blnok) {
        if (next_item.cat != SCAT_MUSIC) {
            log_message("Found non-music item " + next_item.strmedia +
                        ", using it");
            blnok = true;
        }
    }

    // Item is automatically ok if it is LineIn)
    // (unlike MU MP3 files, we don't check for too much repetition, attempt re-ordering, etc)
    if (!blnok) {
        if (next_item.cat == SCAT_MUSIC) {
            if (next_item.strmedia == "LineIn") {
                log_message("Found LineIn item, using it");
                blnok = true;
            }
        }
    }

    // Check if the item should be skipped
    bool blnskip = false;
    string strskip_reason = "";

    if (!blnok) {
        // Was the song played recently?
        if (!blnskip) {
            if (music_history.song_played_recently(
                next_item.strmedia,
                intmin_songs_before_song_repeat)) {
                // Song was played recently
                blnskip = true;
                strskip_reason = "it was played recently";
            }
        }

        // Otherwise, was the artist played recently?
        if (!blnskip) {
            // Get the song artist
            string artist = mp3tags.get_mp3_artist(next_item.strmedia);

            // How many unique artists are there remaining in the current
            // music playlist? (current = without reverting, but possibly
            //                  with playlist looping, if allowed by the
            //                  segment)
            int num_artists = run_data.current_segment->
                count_remaining_playlist_artists(mp3tags);

            // Did a song by this artist play recently?
            if (music_history.artist_song_played_recently(
                artist, num_artists-1, mp3tags)) {
                blnskip = true;
                strskip_reason = "a song by the same artist (" + artist +
                                 ") was played recently";
            }
        }
    }

    // Skip?
    if (blnskip) {
      intnum_skipped_songs++;
      string strdescr = "<ERROR>";
      try {
        strdescr = mp3tags.get_mp3_description(next_item.strmedia);
      } catch_exceptions;
      log_debug("Skipping song, " + strskip_reason + ": \"" +
                next_item.strmedia + "\" - \"" + strdescr + "\"");
    }

    // If we didn't skip the song, then it is ok:
    if (!blnskip) {
        blnok = true;
    }

    // One less attempt remaining:
    --intattempts_left;
  }

  // Log how many songs were skipped to the main (non-debugged) log, if
  // we skipped any:
  if (intnum_skipped_songs > 0) {
    log_message("Skipped " + itostr(intnum_skipped_songs) +
                " songs while finding songs that haven't played recently. See"
                " debug log for more info.");
  }

  // Did we find an item which isn't a recently-played song?
  if (!blnok) {
    // No. Our logic has failed for some reason so reset it.
    log_warning("Forced to clear the in-memory (not database) music history!");
    music_history.clear();
    my_throw("I was unable to find a song which has not been played recently!");
  }
}
