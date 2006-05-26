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

  // Check if the item is already loaded:
  if (item.blnloaded) my_throw("Item is already loaded! Can't load it again! Reset it first!");

  // Reset "next_item"
  item.reset();

  // If the current promo batch queue is not empty, then fetch the next item:
  if (!item.blnloaded && run_data.waiting_promos.size() != 0) {
    get_next_item_promo(item, intstarts_ms);
  }

  // Update the store status, check if the store is currently open or closed:
  load_store_status();

  // Stop playback if we are within store hours:
  if (!item.blnloaded && !store_status.blnopen) {
    // Store is closed. Next item is silence.
    item.cat = SCAT_SILENCE;
    item.blnloaded = true;
    // Reset our "segments delayed by" factor:
    run_data.intsegment_delay=0;
  }

  // Return the next promo if there is one waiting in the database:
  if (!item.blnloaded) {
    // Inside store hours. Any promos?
    get_next_item_promo(item, intstarts_ms);
  }

  if (!item.blnloaded) {
    // No promos. Use Format Clocks to determine the next item.
    get_next_item_format_clock(item, intstarts_ms);
  }

  // Check if we found something to play:
  if (!item.blnloaded) my_throw("Could not find the next item!");
}

// Functions called by get_next_item():
void player::get_next_item_promo(programming_element & item, const int intstarts_ms) {
  if (blndebug) cout << "Checking if there is a promo (regular Radio Retail announcement) to play..." << endl;

  // Are there any promos waiting to be returned?
  if (run_data.waiting_promos.size() != 0) {
    if (blndebug) cout << "Using a promo from a previously retrieved batch" << endl;
    // Yes: Return the promo and then leave this function
    item = run_data.waiting_promos[0];
    run_data.waiting_promos.pop_front();
    return;
  }

  // No promos waiting to be returned.

  // Timing variables:
  static datetime dtmlast_now         = datetime_error; // Used to check for system clock changes
  static datetime dtmlast_run         = datetime_error; // The last time the function's main logic ran.
  static datetime dtmlast_promo_batch = datetime_error; // The last time a promo batch was returned.

  // Check if the system clock was set back in time since the last time:
  datetime dtmnow = now();
  if (dtmnow < dtmlast_now) {
    testing;
    log_message("System clock change detected, recallibrating function timing...");
    dtmlast_run         = datetime_error;
    dtmlast_promo_batch = datetime_error;
  }

  // Check that enough time has passed since the last time this function was called:
  if (dtmlast_run != datetime_error && (dtmnow/30 == dtmlast_run/30)) {
    if (blndebug) cout << "Function was called within the last 30 seconds, not running main logic..." << endl;
    return;
  }

  // Now remember the last time we ran:
  dtmlast_run = dtmnow;

  // Now check that the minimum time has passed since the last announcement batch

  // Check if any ads can play now (ie, in a regular batch), or only adverts which have a specific "forced" time
  // (these could end up in "batches" also, but they take precedence over the artificial forced waits between
  // batches
  bool blnAdBatchesAllowedNow = run_data.current_segment.blnloaded &&
                                run_data.current_segment.blnpromos &&
                                (dtmnow >= dtmlast_promo_batch + 60 * config.intmin_mins_between_batches);

  if (blndebug) {
    if (blnAdBatchesAllowedNow) {
      cout << "Regular advert batches are allowed to play now." << endl;
    }
    else {
      cout << "Regular advert batches are not allowed to play now." << endl;
      if (!run_data.current_segment.blnloaded) cout << " - Format Clock segment not yet loaded." << endl;
      if (!run_data.current_segment.blnpromos) cout << " - Segment does not allow promos." << endl;
      if (!(dtmnow >= dtmlast_promo_batch + 60 * config.intmin_mins_between_batches)) cout << " - Last promo batch played too recently." << endl;
      cout << "However, promos with 'forced' times will still be played." << endl;
    }
  }

  // Correct announcements that were previously interrupted during playback...
  correct_waiting_promos();

  // Declare the list of items to be played. This list is used mainly for
  // Checking that ads to play do not have the same mp3 or catagory.
  TWaitingAnnouncements AnnounceList;
  TWaitingAnnouncements AnnounceMissed_SameVoice; // These are the announcements missed because their

  string psql_EarliestTime = time_to_psql(time()+(intstarts_ms/1000)-(60*config.intmins_to_miss_promos_after));

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
                 "  tblSchedule_TZ_Slot.dtmDay = date '" + format_datetime(now() + intstarts_ms/1000, "%F")  + "'" + " AND "
                 "  tblSchedule_TZ_Slot.bitScheduled = " + itostr(ADVERT_SNS_LOADED) + " AND "
                 "("
                     "("
                       "(tblSchedule_TZ_Slot.dtmForcePlayAt >= " + psql_EarliestTime + ") AND "
                       "(tblSchedule_TZ_Slot.dtmForcePlayAt <= " + psql_time + ")"
                     ")";

  // The query above only asks for "forced" playback times. Now if they are allowed now, then also include an
  // "OR" which will include the regular, un-forced times.
  if (blnAdBatchesAllowedNow) {
    strSQL += " OR "
                   "("
                     "(tblSchedule_TZ_Slot.dtmForcePlayAt IS NULL) AND"
                     "(tblSlot_Assign.dtmStart >= " + psql_EarliestTime + ") AND "
                     "(tblSlot_Assign.dtmStart <= " + psql_time + ")"
                   ")";
  }

  // Now close the "WHERE" block and append the rest of the query.
  strSQL += ") ORDER BY tblSched.strPriorityConverted, tblSlot_Assign.dtmStart, tblSchedule_TZ_Slot.lngTZ_Slot";

  // - Player v6.15 - Now the announcement priority code (CA=1,SP=2,AD=3) actually has an effect on the announcement
  // playback priority (ORDER BY tblSched.strPriorityConverted)
  if (blndebug) cout << "Querying database for adverts. SQL: " << strSQL << endl;
  pg_result RS = db.exec(strSQL);
  if (blndebug) cout << "Returned rows: " << RS.size() << endl;

  // 6.14: This loop is now where announcement limiting takes place
  while (RS && AnnounceList.size() < (unsigned) config.intmax_promos_per_batch) {
    string strdbPos, scPriority, scPriorityConv, scFileName, tmplngTZslot, strProductCat, strPlayAtPercent, strAnnCode;
    datetime dtmTime;

    strdbPos = strProductCat = scPriority = scPriorityConv = scFileName
             = strPlayAtPercent = strAnnCode = "";

    strdbPos         = RS.field("lngTZ_Slot", "");
    strProductCat    = RS.field("strProductCat", "");

    dtmTime          = parse_psql_time(RS.field("dtmForcePlayAt", RS.field("dtmStart", "").c_str()));

    // Check: Do we set the "Forced time advert" flag?
    bool blnForcedTime = !RS.field_is_null("dtmForcePlayAt");

    scPriority       = RS.field("strPriorityOriginal", "");
    scPriorityConv   = RS.field("strPriorityConverted", "");
    scFileName       = lcase(RS.field("strFileName", ""));
    strPlayAtPercent = RS.field("strPlayAtPercent", "");
    strAnnCode       = RS.field("strAnnCode", "");

    // v6.14.3 - PAYB stuff:
    string strPrerecMediaRef = lcase(RS.field("strprerec_mediaref", ""));
    bool blnCheckPrerecLifespan = (RS.field("bitcheck_prerec_lifespan", "0") == "1");

    // Interpret strPlayAtPercent
    if (isint(strPlayAtPercent)) {
      // Clip the value from 0 to 100
      if (strtoi(strPlayAtPercent) > 100) {
        testing;
        strPlayAtPercent="100";
      }
      else if (strtoi(strPlayAtPercent) < 0) {
        testing;
        strPlayAtPercent = "0";
      }
    }
    else {
      testing;
      // Convert playback volume percentage to upper case
      strPlayAtPercent = ucase(strPlayAtPercent);

      if (strPlayAtPercent != "MUS" && strPlayAtPercent != "ADV") {
        testing;
        strPlayAtPercent = "100";
      }
    }

    // Get the path of the announcement that wants to play now, it's actual filename at that path, and whether the file exists
    // on the system...
    string strFilePath = "";
    string strFilePrefix = lcase(substr(scFileName, 0, 2));

    if (strFilePrefix == "ca") {
      strFilePath = config.dirs.strannouncements; // Announcements
    }
    else if (strFilePrefix == "sp") {
      strFilePath = config.dirs.strspecials; // Specials
    } else if (strFilePrefix == "ad") {
      strFilePath = config.dirs.stradverts; // Adverts
    }
    else {
      log_error ("Advert filename " + scFileName + " has an unknown prefix " + strFilePrefix);
      strFilePath = config.dirs.strmp3; // Default to the music folder
    }

    string strActualFileName = "";   // This is the filename of a matching file (matching meaning there
                                     // is a case-non-sensitive match of filenames

    bool blnSkipItem = false; // Set to true if the announcement is to be skipped

    if (!file_existsi(strFilePath, scFileName, strActualFileName)) {
      // MP3 not found. Is there an encrypted version of the file instead?
      if (!file_existsi(strFilePath, scFileName + ".rrcrypt", strActualFileName)) {
        // Nope, there isn't an encrypted version either.
        log_error("Could not find announcement MP3: " + strFilePath + scFileName);
        blnSkipItem = true;
      }
    }

    // v6.14.3 - PAYB stuff:
    if (blnCheckPrerecLifespan) {
      if (strPrerecMediaRef == "") {
        testing;
        // The bit for checking the lifespan is set, but the media reference field is empty
        log_error("tblsched.bitcheck_prerec_lifespan set to 1, but tblsched.strprerec_mediaref is empty!");
        blnSkipItem = true;
      }
      else {
        // strprerec_mediaref and bitcheck_prerec_lifespan are set. Retrieve lifespan and global expiry date info from
        // the related tblprerec_item record
        string psql_PrerecMediaRef = psql_str(lcase(strPrerecMediaRef));
        strSQL = "SELECT intglobalexp, intlifespan FROM tblprerec_item WHERE lower(strmediaref) = " + psql_PrerecMediaRef;

        pg_result rsPrerecItem = db.exec(strSQL);
        // Check the results of the query.
        if (rsPrerecItem.size() != 1) {
          // We expected to find 1 matching record, but a different number was found
          log_error(itostr(rsPrerecItem.size()) + " prerecorded items match media reference " + strPrerecMediaRef + ". Cannot play " + scFileName);
          blnSkipItem = true;
        }
        else {
          // We found 1 matching record, and found it. Now check the current date against the listed global expiry date, and
          // the current lifespan, of the prerecorded item.
          int intglobalexp = strtoi(rsPrerecItem.field("intglobalexp", "-1"));
          int intlifespan = strtoi(rsPrerecItem.field("intlifespan", "-1"));

          // Get today's rr date, as an integer.
          int inttoday_rrdate = get_rrdateint(date());

          // Check the global expiry date
          if ((intglobalexp != -1) && (intglobalexp < inttoday_rrdate)) {
            testing;
            // Global expiry date has elapsed!
            log_error("Advert skipped because because it's global expiry date has passed: " + scFileName);
            blnSkipItem = true;
          }
          // Check the lifespan.
          else if ((intlifespan != -1) && (intlifespan < inttoday_rrdate)) {
            testing;
            // Lifespan has elapsed!
            log_error("Advert skipped because the period it was purchased for has expired: " + scFileName);
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
        blnSkipItem = ((*item).strFileName==scFileName) || (strProductCat != "" && (*item).strProductCat==strProductCat);
        ++item;
      }

      // . Check 2: Do not allow ads by the same announcer to play twice in succession:
      if (!blnSkipItem && strAnnCode != "") {
        if (AnnounceList[AnnounceList.size()-1].strAnnCode == strAnnCode) {
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
      TWaitingAnnounce Announce;

      Announce.dbPos = strtoi(strdbPos);
      Announce.strFileName = scFileName;
      Announce.strProductCat = strProductCat;
      Announce.dtmTime = dtmTime;

      Announce.blnForcedTime = blnForcedTime;
      Announce.strPriority = scPriority;
      Announce.strPlayAtPercent = strPlayAtPercent;
      Announce.strAnnCode = strAnnCode;
      Announce.strPath = strFilePath;

      if (!blnSkipItem) {
        // This announcement will be played, queue it in the list of announcements to be played
        AnnounceList.push_back(Announce);
      }
      else {
        // This announcement possibly will not play, because the announcer was the same as the last
        // announcer in the "to-play" queue. We will try later to add it to the list anyway (if the maximum
        // allowed number of announcements per batch has not yet been reached)
        AnnounceMissed_SameVoice.push_back(Announce);
      }
    }
    RS++; // Move to the next record...
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
              if ((*remove_item).dbPos==test_dbPos) {
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

  bool blnForcedTimeAdToPlay = false; // Set to true if there is a "forced to play at time" advert to be played.

  // Now, we have the final list of announcements to be played. Mark these announcements in the database
  // as "listed to be played", and log a message in the player log...
  TWaitingAnnouncements::const_iterator announce_item = AnnounceList.begin();
  while (announce_item!=AnnounceList.end()) {
    // Log a message to say that this announcement is enqued
    string strTime = format_datetime((*announce_item).dtmTime, "%T");
    string strDate = format_datetime(date(), "%F");
    log_message("Announcement to be played: " + (*announce_item).strFileName +
                               ", priority: " + (*announce_item).strPriority +
                               ", catagory: \"" + (*announce_item).strProductCat +
                               "\", volume: " + (*announce_item).strPlayAtPercent +
                               "%,  time: " + strTime +
                               ", date: " + strDate +
                               ", db index: " + itostr((*announce_item).dbPos));

    // Update the database also
    strSQL = "UPDATE tblSchedule_TZ_Slot SET "
               "bitScheduled = " + itostr(ADVERT_LISTED_TO_PLAY) +
               ", dtmScheduledAtDate = " + psql_date +
               ", dtmScheduledAtTime = " + psql_time +
               " WHERE lngTZ_Slot = " + itostr((*announce_item).dbPos);
    db.exec(strSQL);

    // Set a flag if this batch to be played, includes a "force to play at time" advert.
    blnForcedTimeAdToPlay = blnForcedTimeAdToPlay || (*announce_item).blnForcedTime;

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
          promo.blnloaded = true;
          run_data.waiting_promos.push_back(promo);
        }
      }
    }

    // Only if a regular (non-time-forced) advert batch played now, reset the time when the last
    // advert batch stopped playing. This is because we ignore forced-time playback, if for eg
    // The min time between announcemnt batches is 5 minutes, then "forced-time" playbacks
    // can occur in the middle of the 5 minutes, without affecting when the next regular announcement batch
    // will play
    if (blnAdBatchesAllowedNow) {
      dtmlast_promo_batch = now(); // The last announcement of the batch just finished playing.
                                   // So we grab the current time to ensure a minimum amount of music
                                   // before the next set of announcements can play.
    }

    // Now return a promo to the calling func:
    item = run_data.waiting_promos[0];
    run_data.waiting_promos.pop_front();
  }
}

void player::get_next_item_format_clock(programming_element & next_item, const int intstarts_ms) {
  // Fetch info about the "next" item to be played, according to the Format Clock.
  // Also handle all segment timing (eg: last segment played for too long, etc.

  // Fetch the "real" time when the next item will start playing (in seconds):
  datetime dtmnext_starts = now() + intstarts_ms/1000;

  // Work out the current delays.

  // Do we currently have a segment loaded?
  if (run_data.current_segment.blnloaded) {
    // We now have the time when the next item will start.
    // Work out if the current item will play past the end of the current segment.
    // This logic works by assuming that the current item ends on the second just before
    // the next item starts. Based on that assumption, does the current item end after
    // the current segment ends? If so, by how many seconds?
    datetime dtmseg_end = run_data.current_segment.dtmstart + run_data.current_segment.intlength - 1;
    if (dtmnext_starts > dtmseg_end + 1) {
      // Yes: Increase the segment delay factor
      int intdiff = dtmnext_starts - dtmseg_end - 1; // Take out that extra second here.
      int intnew_segment_delay = run_data.intsegment_delay + intdiff;
      if (intnew_segment_delay > intmax_segment_push_back) {
        log_warning("I have to drop " + itostr(intnew_segment_delay - intmax_segment_push_back) + "s of segment playback time! I've reached my segment 'delay' limit of " + itostr(intmax_segment_push_back) + "s");
        intnew_segment_delay = intmax_segment_push_back;
      }
      log_line("Current item is going to end " + itostr(intdiff) + "s after the current segment end. Increasing segment delay factor to " + itostr(intnew_segment_delay) + "s (+" + itostr(intnew_segment_delay - run_data.intsegment_delay) + "s)");
      run_data.intsegment_delay = intnew_segment_delay;
    }
  }

  // Check here: has the hour changed?
  // If it has, then we reset the current segment delay factor.
  {
    static datetime dtmlast_checked = datetime_error;

    // Has the hour changed?
    datetime dtmnow = now();

    if (dtmlast_checked != datetime_error && dtmlast_checked/(60*60) != dtmnow/(60*60) && run_data.intsegment_delay > 0) {
      // Don't bother logging warnings if format clocks are disabled:
      if (config.blnformat_clocks_enabled) {
        log_warning("Hour has changed. Resetting segment delay (currently: " + itostr(run_data.intsegment_delay) + "s)");
        log_warning("All segments that were scheduled to play between '" + format_datetime(dtmnext_starts - run_data.intsegment_delay, "%T") + "' and '" + format_datetime((dtmnext_starts) - 1, "%T") + "' will be missed!");
      }
      run_data.intsegment_delay = 0;
    }

    // We've done the check now (or not, if this is the first time the function was called)
    // Wait until the next hour:
    dtmlast_checked = dtmnow;
  }

  // And now get our "delayed" time, the time in the past at which we fetch format clock data
  datetime dtmdelayed = dtmnext_starts - run_data.intsegment_delay;

  // Reset info currently in the item:
  next_item.reset(); // Reset info currently in the item.

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
                      "date '" + strfc_date + "' BETWEEN COALESCE(tblfc_sched.dtmstart, '0000-01-01') AND "
                                                        "COALESCE(tblfc_sched.dtmend, '9999-12-25') AND "
                      "time '" + strfc_time_with_hour + "' BETWEEN tblfc_sched_day.dtmstart AND tblfc_sched_day.dtmend "
                      "ORDER BY tblfc_sched.lngfc_sched DESC LIMIT 1";
      pg_result rs = db.exec(strsql);

      // How many results?
      if (rs.size() <= 0) { // No format clocks scheduled.
        log_warning("No Format Clocks scheduled for this hour. Will revert to the default Format Clock.");
      } else { // User scheduled 1 or more format clocks.
        // We found a user-scheduled format clock. Fetch the segment:
        lngfc = strtol(rs.field("lngfc"));
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
        pg_result rs = db.exec(strsql);
        // Did we find 1 record?
        if (rs.size() != 1) {
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

  // - 'Segment expired' means that the a segment's time has run out.
  bool blnsegment_expired = dtmnext_starts > (run_data.current_segment.dtmstart + run_data.current_segment.intlength - 1);

  if (!run_data.current_segment.blnloaded ||
       lngfc_seg != run_data.current_segment.lngfc_seg ||
       run_data.blnforce_segment_reload ||
       blnsegment_expired) {

    // Reset the "force segment reload" control variable:
    run_data.blnforce_segment_reload = false;

    // Load the new segment:
    log_message("Loading Format Clock segment (id: " + itostr(lngfc_seg) + ")");

    // A -1 lngfc_seg means load the currently-scheduled music profile instead
    run_data.current_segment.load_from_db(db, lngfc_seg, dtmdelayed, config);

    // How far into the segment did we query for?
    int intdiff = dtmdelayed - run_data.current_segment.scheduled.dtmstart;

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
    run_data.current_segment.dtmstart  = dtmnext_starts;
    run_data.current_segment.intlength = run_data.current_segment.scheduled.dtmend - dtmdelayed + 1;

    // Reclaim "delayed by" time if this is a music segment. But leave at least 60 seconds.
    if (run_data.current_segment.cat.cat == SCAT_MUSIC && run_data.current_segment.intlength > 60) {
      int intnew_length = run_data.current_segment.intlength - run_data.intsegment_delay;
      if (intnew_length < 60) {
        intnew_length = 60;
      }
      int intdiff = run_data.current_segment.intlength - intnew_length;
      if (intdiff > 0) {
        // Hooray, we get to reclaim some space
        log_line("Reclaiming " + itostr(intdiff) + "s of 'segment delayed' time from the current (music) segment.");
        run_data.intsegment_delay -= intdiff;
        run_data.current_segment.intlength -= intdiff;
      }
    }

    // Now log some basic details about the new segment.
    log_line(run_data.current_segment.cat.strname + " segment will play between "
      + format_datetime(run_data.current_segment.dtmstart, "%T") + " and "
      + format_datetime(run_data.current_segment.dtmstart
      + run_data.current_segment.intlength - 1, "%T")
      + " (" + itostr(run_data.current_segment.intlength) + "s)");

    // If this is a music playlist then log it to the database:
    if (run_data.current_segment.cat.cat == SCAT_MUSIC) {
      // Log the XMMS playlist, and the system's available music later:
      run_data.blnlog_all_music_to_db = true;
    }
  }
  else {
    // Segment has not changed. Just log the current segment and end time.
    log_message("No segment change. " + run_data.current_segment.cat.strname + " segment (id: " + itostr(run_data.current_segment.lngfc_seg) + ") is scheduled to play until " + format_datetime(run_data.current_segment.dtmstart + run_data.current_segment.intlength - 1, "%T"));
  }

  // Now fetch the next item to play, from the segment. Make sure it isn't a
  // song which was played recently:
  get_next_item_not_recent_music(next_item, intstarts_ms);
}

void player::get_next_item_not_recent_music(programming_element & next_item, const int intstarts_ms) {
  // This function stops songs from playing too soon after each other.
  // eg: segment changes from music to non-music and back to music.
  bool blnok           = false; // Set to true when we find an item which isn't a recently-played song

  // Count the music items in the current playlist:
  int intnum_music_items = run_data.current_segment.count_items_from_catagory(SCAT_MUSIC);

  // Determine the minimum number of music items that should elapse before
  // a song can repeat
  // - intprevent_song_repeat_factor represents a percentage of the current
  //   music playlist's length.
  int intmin_songs_before_song_repeat = (intnum_music_items * intprevent_song_repeat_factor) / 100;

  // Attempt [current playlist length * 2] (or 100, whichever is higher) times to fetch
  // an item which hasn't been played recently. Using * 2 because it is possible for the
  // playlist to change during this process (eg, a music segment with repeating disabled,
  // and the system reverts to a music profile instead
  int intattempts_left = run_data.current_segment.programming_elements.size() * 2;
  if (intattempts_left < 100) intattempts_left = 100;

  while (!blnok && intattempts_left > 0) {
    // Fetch the next item:
    run_data.current_segment.get_next_item(next_item, db, intstarts_ms, config);
    // Is the item ok to use?
    blnok = next_item.cat != SCAT_MUSIC || !m_music_history.song_played_recently(next_item.strmedia, intmin_songs_before_song_repeat);

    // If the item is not ok to use, then log that it was skipped:
    if (!blnok) {
      string strdescr = "<ERROR>";
      try {
        strdescr = mp3tags.get_mp3_description(next_item.strmedia);
      } catch_exceptions;
      log_message("Skipping song, it was played recently: \"" + next_item.strmedia + "\" - \"" + strdescr + "\"");
      // If this check failed then go to the next attempt:
      if (!blnok) --intattempts_left;
    }
  }

  // Did we find an item which isn't a recently-played song?
  if (!blnok) {
    // No. Our logic has failed for some reason so reset it.
    log_warning("Forced to clear the in-memory (not database) music history!");
    m_music_history.clear();
    my_throw("I was unable to find a song which has not been played recently!");
  }
}
