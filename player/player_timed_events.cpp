/***************************************************************************
                          player_timed_events.cpp  -  description
                             -------------------
    begin                : Thu Jan 8 2004
    copyright            : (C) 2004 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// This file holds the declarations for player events which are called at intervals.
 
#include "player_timed_events.h"
#include "temp_dir.h"
#include "rr_security.h"
#include "player.h"

void event_check_music::run() {
  Player.DB.check();
  // Check the media player status. But first check for any commands or volume level changes.

  // Before checking the media player, check the DB for commands that could
  // affect the current status (eg: The wizard inserts a Pause command and
  // then pauses XMMS. If this is the case, the player must not restart playback.
  // because it incorrectly thinks that music should be playing now)
  Player.Process_WaitingCMDs();

  // Some things to do only if the music playback is enabled, ie the player is not paused or stopped
  //

  // If we're in a "back to back" announcement state, then don't attempt to startup music:
  if (!Player.do_next_announce_batch_immediately()) {
    if (Player.PlaybackEnabled()) {
      if (Player.CheckMusicProfile()) {
        Player.MediaPlayer_Play();
      }
      // Log any new music being played to the music history
      Player.LogLatestMusicMP3Played();
    }
  }

  // Regardless of playback being enabled, do a MediaPlayer maintenance check, and
  // check the volumes
  Player.volZones(); // Check the volumes also - in case of changed tblstore values. - It is important
                     // to do this before the mediaplayer maintenance check!
  Player.MediaPlayer.MaintenanceCheck(); // Check for altered volumes, do other maintenance...
}

void event_check_received::run() {
  // Check regularly for any new files in the received folder
  Player.DB.check();
  Player.Check_Received();
}

void event_check_waiting_cmds::run() {
  Player.DB.check();
  Player.Process_WaitingCMDs();
}

void event_operational_check::run() {
  // Update LiveInfo.chk, check the store's "CLOSED/OPEN" status, remove old DB items, etc, etc, etc
  Player.DB.check();

  // Update the live-info file every 10 minutes;
  if (intUpdateLiveInfoCnt >= 20) {
    intUpdateLiveInfoCnt = 0;
    Player.doUpdateLiveInfo();
  }
  else intUpdateLiveInfoCnt++;

  // Check the store's OPEN/CLOSED status.
  string gstrSemiSonic = Player.SemiSonic();

  if (gstrSemiSonic=="OPEN" && Player.CurrentStatus.StoreStatus != "OPEN")
    Player.StoreOpen();
  else if (gstrSemiSonic=="CLOSE" && Player.CurrentStatus.StoreStatus != "CLOSE")
    Player.StoreClose();

  // If the music playlist changed, then the global variable [blnMusicLoggingNeeded] is set. Here is where
  // we actually log the XMMS playlist, and log all the available music on the machine.
  if (Player.blnMusicLoggingNeeded) {
    // Log an informative message.
    log_message("Music playlist was updated, logging to database now.");
    // Log the playlist to the DB
    Player.LogXMMSPlaylistToDB();
    // Also do a quick scan of all the music on the machine, and log this to the database
    Player.LogAllMachineMusicToDB();
    // We have done the logging, reset the flag variable
    Player.blnMusicLoggingNeeded = false;
  }
}

void event_scheduler::run() {
  // This function checks if there are announcements to play now, and also plays them.
  // - Also there are checks to determine when the player clicks on the 'pause' and 'stop' XMMS buttons.
  // - when this happens it is possible that the player also has a "stop" or "pause" instruction.
  Player.DB.check();
  
  // Don't look for announcements to play, if music & ad playback are currently disabled
  if (!Player.PlaybackEnabled()) return;

  // Check that the time was not set back.
  DateTime dtmNow = Now();
  if (dtmNow < dtmLast_timSched_Now) {
    log_message("System clock change detected, recallibrating announcement scheduling...");
    dtmLastAdBatch = datetime_error;
  }
  dtmLast_timSched_Now = dtmNow; // Now store the current "Now" value for later testing.

  // Now check that the minimum time has passed since the last announcement batch

  // Check if any ads can play now (ie, in a regular batch), or only adverts which have a specific "forced" time
  // (these could end up in "batches" also, but they take precedence over the artificial forced waits between
  // batches
  bool blnAdBatchesAllowedNow = (dtmNow >= dtmLastAdBatch+60*Player.Config.intMinMinsBetweenAdBatches);

  // Check if there are any commands that could affect announcement playback, eg Player Stop or Player Pause.
  Player.Process_WaitingCMDs();

  // Now quit if the player is disabled at this point (maybe there was a MPST command)
  if (!Player.PlaybackEnabled()) return;

  // Correct announcements that were previously interrupted during playback...
  Player.CorrectWaitingAnnouncements();

  // Declare the list of items to be played. This list is used mainly for
  // Checking that ads to play do not have the same mp3 or catagory.
  TWaitingAnnouncements AnnounceList;
  TWaitingAnnouncements AnnounceMissed_SameVoice; // These are the announcements missed because their

  string psql_EarliestTime = TimeToPSQL(Time()-(60*Player.Config.intMinsToMissAdsAfter));

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
                 "  tblSchedule_TZ_Slot.dtmDay = date '" + format_datetime(Now(), "%yyyy-%mm-%dd")  + "'" + " AND "
                 "  tblSchedule_TZ_Slot.bitScheduled = " + itostr(AdvertSNSLoaded) + " AND "
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

  pg_result RS = Player.DB.exec(strSQL);
  long lngdb_scheduled_advert_count=RS.recordcount(); // The number of adverts listed in the database that
                                                      // "want" to be played now.

  // 6.14: This loop is now where announcement limiting takes place
  while (!RS.eof() && AnnounceList.size() < (unsigned) Player.Config.intMaxAdsPerBatch) {
    string strdbPos, scPriority, scPriorityConv, scFileName, tmplngTZslot, strProductCat, strPlayAtPercent, strAnnCode;
    DateTime dtmTime;

    strdbPos = strProductCat = scPriority = scPriorityConv = scFileName
             = strPlayAtPercent = strAnnCode = "";

    strdbPos         = RS.field("lngTZ_Slot", "");
    strProductCat    = RS.field("strProductCat", "");

    dtmTime          = parse_psql_time(RS.field("dtmForcePlayAt", RS.field("dtmStart", "").c_str()));

    // Check: Do we set the "Forced time advert" flag?
    bool blnForcedTime = !RS.field_is_null("dtmForcePlayAt");

    scPriority       = RS.field("strPriorityOriginal", "");
    scPriorityConv   = RS.field("strPriorityConverted", "");
    scFileName       = StringToLower(RS.field("strFileName", ""));
    strPlayAtPercent = RS.field("strPlayAtPercent", "");
    strAnnCode       = RS.field("strAnnCode", "");

    // v6.14.3 - PAYB stuff:
    string strPrerecMediaRef = StringToLower(RS.field("strprerec_mediaref", ""));
    bool blnCheckPrerecLifespan = (RS.field("bitcheck_prerec_lifespan", "0") == "1");

    // Interpret strPlayAtPercent
    if (IsInt(strPlayAtPercent)) {
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
      strPlayAtPercent = StringToUpper(strPlayAtPercent);

      if (strPlayAtPercent != "MUS" && strPlayAtPercent != "ADV") {
        strPlayAtPercent = "100";
      }
    }

    // Get the path of the announcement that wants to play now, it's actual filename at that path, and whether the file exists
    // on the system...
    string strFilePath = "";
    string strFilePrefix = StringToLower(substr(scFileName, 0, 2));

    if (strFilePrefix == "ca") {
      strFilePath = Player.Config.AppPaths.strannouncements; // Announcements      
    }
    else if (strFilePrefix == "sp") {
      strFilePath = Player.Config.AppPaths.strspecials;  // Specials      
    } else if (strFilePrefix == "ad") {
      strFilePath = Player.Config.AppPaths.stradverts; // Adverts
    }
    else {
      log_error ("Advert filename " + scFileName + " has an unknown prefix " + strFilePrefix);
      strFilePath = Player.Config.AppPaths.strmp3; // Default to the music folder
    }

    string strActualFileName = "";   // This is the filename of a matching file (matching meaning there
                                     // is a case-non-sensitive match of filenames

    bool blnSkipItem = false; // Set to true if the announcement is to be skipped

    if (!FileExists_CaseInsensitive(strFilePath, scFileName, strActualFileName)) {
      // MP3 not found. Is there an encrypted version of the file instead?
      if (!FileExists_CaseInsensitive(strFilePath, scFileName + ".rrcrypt", strActualFileName)) {
        // Nope, there isn't an encrypted version either.
        log_error("Could not find announcement MP3: " + strFilePath + scFileName);
        blnSkipItem = true;
      }
    }

    // v6.14.3 - PAYB stuff:
    if (blnCheckPrerecLifespan) {
      if (strPrerecMediaRef == "") {
        // The bit for checking the lifespan is set, but the media reference field is empty
        log_error("tblsched.bitcheck_prerec_lifespan set to 1, but tblsched.strprerec_mediaref is empty!");
        blnSkipItem = true;
      }
      else {
        // strprerec_mediaref and bitcheck_prerec_lifespan are set. Retrieve lifespan and global expiry date info from
        // the related tblprerec_item record
        string psql_PrerecMediaRef = psql_str(StringToLower(strPrerecMediaRef));
        strSQL = "SELECT intglobalexp, intlifespan FROM tblprerec_item WHERE lower(strmediaref) = " + psql_PrerecMediaRef;

        pg_result rsPrerecItem = Player.DB.exec(strSQL);
        // Check the results of the query.
        if (rsPrerecItem.recordcount() != 1) {
          // We expected to find 1 matching record, but a different number was found
          log_error(itostr(rsPrerecItem.recordcount()) + " prerecorded items match media reference " + strPrerecMediaRef);
          blnSkipItem = true;
        }
        else {
          // We found 1 matching record, and found it. Now check the current date against the listed global expiry date, and
          // the current lifespan, of the prerecorded item.
          int intglobalexp = strtoi(rsPrerecItem.field("intglobalexp", "-1"));
          int intlifespan = strtoi(rsPrerecItem.field("intlifespan", "-1"));

          // Get today's rr date, as an integer.
          int inttoday_rrdate = GetRRDateInt(Date());

          // Check the global expiry date
          if ((intglobalexp != -1) && (intglobalexp < inttoday_rrdate)) {
            // Global expiry date has elapsed!
            log_error("Advert skipped because because it's global expiry date has passed: " + scFileName);
            blnSkipItem = true;
          }
          // Check the lifespan.
          else if ((intlifespan != -1) && (intlifespan < inttoday_rrdate)) {
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
    RS.movenext(); // Move to the next record...
  }

  // We've reached either the end of the recordset, or the limit for number of announcements to play in a single batch.

  // If we have not reached the maximum number of allowed announcements, but there were announcements skipped
  // because their announcer was the same as the previously-queued announcer, then...
  if (AnnounceList.size() < (unsigned) Player.Config.intMaxAdsPerBatch &&
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
                AnnounceList.size() < (unsigned) Player.Config.intMaxAdsPerBatch) {
        // Temporarily append the skipped announcement to the end of the "to-play" list
        //    . The item will stay in the queue if a valid playlist order is found, otherwise it will be removed later..

        AnnounceList.push_back(*item);
        // -- remember the DB ID of the announcement in case we mess up and don't get the permutations correct...
        unsigned long test_dbPos = item->dbPos;

        // - Calculate the number of permutations possible with the new "to-play" length
        long lngPermutations=CalcPermutations(AnnounceList.size());

        // Start the permutation-generation... loop
        bool blnValidSequenceFound = false; // set to true if a valid order for the ads is found...
        long lngPermutationNum = 1; // The number of the current permutiation out of the total possible.

        // Init variables used for generating permutations...
        long lngTransposePos = 0; // We swap two elements, the one at this pos, and the one just after.
        int intTransposeDir = 1; // After doing a swap, the next permuation will be generated by swapping two adjacent elements
                                              // - eg after swapping element 1 and 2, we swap element 2 and 3. When we reach
                                              // the end of the list, we start moving backwards again - eg, swap 9 and 10, then swap 8 and 9
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
    string strTime = format_datetime((*announce_item).dtmTime, "%HH:%NN:%SS %am/pm");
    string strDate = format_datetime(Date(), "%dd/%mm/%yyyy");
    log_message("Announcement to be played: " + (*announce_item).strFileName +
                               ", priority: " + (*announce_item).strPriority +
                               ", catagory: \"" + (*announce_item).strProductCat +
                               "\", volume: " + (*announce_item).strPlayAtPercent +
                               "%,  time: " + strTime +
                               ", date: " + strDate +
                               ", db index: " + itostr((*announce_item).dbPos));

    // Update the database also
    strSQL = "UPDATE tblSchedule_TZ_Slot SET "
               "bitScheduled = " + itostr(AdvertListedToPlay) +
               ", dtmScheduledAtDate = " + psql_date +
               ", dtmScheduledAtTime = " + psql_time +
               " WHERE lngTZ_Slot = " + itostr((*announce_item).dbPos);
    Player.DB.exec(strSQL);

    // Set a flag if this batch to be played, includes a "force to play at time" advert.
    blnForcedTimeAdToPlay = blnForcedTimeAdToPlay || (*announce_item).blnForcedTime;

    // Move to the next "to play" item..
    ++announce_item;
  }

  // Now start the actual playback if we found any announcements to play...
  if (AnnounceList.size() > 0) {
    // This means that there are ads to play.

    // Now check if it will be appropriate to jump into the next advert batch immediately after this batch ends.
    // - Conditions:
    //    1) The "minimum minutes between batches" setting is set to 0
    //    2) The number of adverts in the current batch is less than the number of ads that
    //        "want" to play now, according to the database.
    if ((Player.Config.intMinMinsBetweenAdBatches == 0) &&
        ((unsigned)lngdb_scheduled_advert_count > AnnounceList.size())) {
      Player.do_next_announce_batch_immediately(true); // Now this event (Advert Scheduler) will be called again immediately.
    }

    // Create a temporary path to decrypt encrypted adverts to:
    temp_dir decrypted_advert_dir(".rrcrypt-" + itostr(rand()));
    
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
      if (!FileExists_CaseInsensitive(Announce.strPath, Announce.strFileName, strActualFileName)) {
        // No. Maybe an encrypted version does exist?
        if (!FileExists_CaseInsensitive(Announce.strPath, Announce.strFileName + ".rrcrypt", strActualFileName)) {
          // Nope, no encrypted version.
          blnFileExists = false;          
          log_error("Could not find announcement MP3: " + Announce.strPath + Announce.strFileName);          
        } else {
          // An encrypted version does exist. Decrypt it into a temporary location:
          cp(Announce.strPath + strActualFileName, (string)decrypted_advert_dir + strActualFileName);
          Announce.strPath = decrypted_advert_dir;
          decrypt_file(Announce.strPath + strActualFileName);

          // Now strip the ".rrcrypt" extension:
          strActualFileName=substr(strActualFileName, 0, strActualFileName.size() - 8);
        }
      }

      // So did we find the file?
      if (blnFileExists) {
        // Announcement MP3 is found. Now calculate the volume to play it at.
        float fltPercentVol=100;

        if (IsInt(Announce.strPlayAtPercent)) {
          // A numeric percentage - calculate the volume
          Player.CurrentStatus.curThisAnnounceVol = Player.CurrentStatus.curDefaultAnnounceVol * strtoi(Announce.strPlayAtPercent) / 100;
          fltPercentVol=strtoi(Announce.strPlayAtPercent);
        }
        else {
          // Check for "ADV", "MUS", or others
          if (Announce.strPlayAtPercent == "MUS")
            fltPercentVol=MUSIC_VOL_PERCENTAGE;
          else if (Announce.strPlayAtPercent == "ADV" ||
                   Announce.strPlayAtPercent == "000")     // 000 is the default padded spaces code from the
                                                           // scheduling monitor - NB - ignore this, don't
                                                           // use a 0 volume.
            fltPercentVol = 100;
          else
            fltPercentVol=100;
        }

        // Queue the announcement to play...
        // - And also pass a function pointer (and a parameter for this function) to call
        // when the announcement is played - this is being done because we want to log exactly
        // what time announcements are played!
        Player.MediaPlayer.QueueAnnouncement(Announce.strPath + strActualFileName, Announce.dbPos, fltPercentVol);
      }
    }

    // All announcements to be played have been queued in MediaPlayer.

    // Now check: Do we wait for the current song to end, or do we launch immediatly into
    // the advert batch?
    bool blnWaitForCurrentSongToEnd = (Player.MediaPlayer.GetMusicType() == xmms) &&
                                      Player.Config.blnAdvertsWaitForSongEnd &&
                                      !blnForcedTimeAdToPlay;
    
    // Kick off the announcement playback now.

    ann_playback_status Status;
    Player.MediaPlayer.StartAnnouncementQueuePlay(Status, blnWaitForCurrentSongToEnd);
    Player.doHandleMediaPlayerAnnPlayback(Status);

    // Only if a regular (non-time-forced) advert batch played now, reset the time when the last
    // advert batch stopped playing. This is because we ignore forced-time playback, if for eg
    // The min time between announcemnt batches is 5 minutes, then "forced-time" playbacks
    // can occur in the middle of the 5 minutes, without affecting when the next regular announcement batch
    // will play
    if  (blnAdBatchesAllowedNow) {
      dtmLastAdBatch = Now(); // The last announcement of the batch just finished playing.
                                               // So we grab the current time to ensure a minimum amount of music
                                               // before the next set of announcements can play.
    }

    // Finally, check for ads that have been meant (they were meant to play but it's been too long since the correct time.
    Player.WriteErrorsForMissedAds();

    // Log the song playing... this is so immediately after media playback resumes, so we have some
    // feedback
    Player.LogLatestMusicMP3Played();

    // Check the volumes at this time..
    Player.volZones();

    // Any remaining MediaPlayer maintenance....
    Player.MediaPlayer.MaintenanceCheck();
  }
}

long event_scheduler::CalcPermutations(long lngNumElements) {
  // Return the number of different permutations possible with a given number of elements.
  // eg - 3 elements can be sorted in 6 different orders.
  long lngresult = lngNumElements;
  while (lngNumElements > 2) {  // eg if lngNumElements is 4 then multiply 4 by 3, then 2.
    --lngNumElements;
    lngresult *=lngNumElements;
  }
  return lngresult;              // return the result...
}

void event_check_db::run() {
  // Check the Player's database connection, is it still up?
  Player.DB.check();
}

void event_player_running::run() {
  Player.DB.check();
  // Display a line (once every minute) showing that the player is running...
  string strLine = "Running...";

  // Is playback enabled?
  if (Player.PlaybackEnabled()) {
    // Playback is enabled. Report which type
    strLine += " (Music type: " + Player.MediaPlayer.GetMusicTypeStr() + ", volume: " + itostr(lrint(Player.MediaPlayer.GetVolume())) + "%)";    
  }
  else {
    // Playback is not enabled
    strLine += " (No music)";
  }
  
  log_line(strLine);
}

// This event is called when there is a database connection error. It is used to check that
// music is playing.
void event_check_music_db_err::run() {
  // Check if the player must *not* check XMMS. This will be the case when the user has been
  // updating XMMS from the Wizard while the database connection is down.
  if (FileExists("/data/radio_retail/progs/player/player.db-error.no-xmms-checks")) {
    log_message("The Player's \"Database Down\" XMMS checks were suspended by the Instore Wizard...");
    return;
  }

  log_message("A database connection error occured. Checking XMMS...");

//    // First check the volumes...
//    if (Player.MediaPlayer.GetVolume() == 0) {
//      // Volume is set to 0, so init some default levels...
//      log_message("Volumes are set to 0, setting default levels...", LOW, CAT_MEDIA_PLAYBACK);
//      Player.MediaPlayer.SetVolumeLevels((125*100)/255, (180*100)/255, (255*100)/255);
//    }

  // Use some default "store open" and "store closed" values...
  log_message("Assuming that store hours are 07:00 - 19:00....");
  if (Time() >= MakeTime(7,0,0) && Time() <= MakeTime(19,0,0)) {
    // Store is probably open. So make sure that music is playing...
    log_message("Store is probably open now, playing music ...");
    Player.MediaPlayer.Play(); // Does not restart playback if already playing...
  }
  else {
    // Store is probably closed. Stop any playing music.
    log_message("Store is probably closed now, stopping music...");
    Player.MediaPlayer.Stop();
  }
};
