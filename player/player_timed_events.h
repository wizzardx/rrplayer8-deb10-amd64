/***************************************************************************
                          player_timed_events.h  -  description
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

 /**
  *@author David Purdy
  */

#ifndef PLAYER_TIMED_EVENTS_H
#define PLAYER_TIMED_EVENTS_H

#include <string>
#include <deque>
#include "event.h"
#include "rr_utils.h"

using namespace std;
using namespace rr;

// This file holds the declarations for player events which are called at intervals.

class player;
  
class event_check_music : public event  {
public:
  // Constructor
  event_check_music(player & pplayer) : Player(pplayer) {};
  // Destructor
  virtual ~event_check_music() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object
};

class event_check_received : public event  {
public:
  // Constructor
  event_check_received(player & pplayer) : Player(pplayer) {};
  // Destructor
  virtual ~event_check_received() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object
};

class event_check_waiting_cmds : public event  {
public:
  // Constructor
  event_check_waiting_cmds(player & pplayer) : Player(pplayer) {};
  // Destructor
  virtual ~event_check_waiting_cmds() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object
};

class event_operational_check : public event  {
public:
  // Constructor
  event_operational_check(player & pplayer) : Player(pplayer) { intUpdateLiveInfoCnt = 0; dtmLastDailyCheck = datetime_error; };
  // Destructor
  virtual ~event_operational_check() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object
  int intUpdateLiveInfoCnt; // A function call counter used for determining when certain logic must run.
  DateTime dtmLastDailyCheck;
};

class event_scheduler : public event  {
public:
  // Constructor
  event_scheduler(player & pplayer) : Player(pplayer) { dtmLast_timSched_Now = dtmLastAdBatch = datetime_error; };
     // - Events have not run yet, set them to far in the past, so that they can run immediately.
  // Destructor
  virtual ~event_scheduler() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object

  DateTime dtmLast_timSched_Now; // Used for internal checking of time changes.
  DateTime dtmLastAdBatch; // The last time an advert batch played;

  // Some types used by the announcement scheduling event...
  struct TWaitingAnnounce {
    unsigned long dbPos;
    string strFileName;
    string strProductCat;

    DateTime dtmTime;
    bool blnForcedTime; // True if dtmTime is a "forced" time. (ie, not a slot in the hour, selected by the sns_loader).
    string strPriority;

    string strPlayAtPercent;
    string strAnnCode;

    string strPath; // The path where this mp3 is found...
  };  
  typedef deque <TWaitingAnnounce> TWaitingAnnouncements;
  
  long CalcPermutations(long lngNumElements);
};

class event_check_db : public event  {
  // Check the Player's database connection, is it still up?
public:
  // Constructor
  event_check_db(player & pplayer) : Player(pplayer) {};
  // Destructor
  virtual ~event_check_db() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object
};

class event_player_running : public event  {
public:
  // Constructor
  event_player_running(player & pplayer) : Player(pplayer) {};
  // Destructor
  virtual ~event_player_running() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object
};
                             
// This event is called when there is a database connection error. It is used to check that
// music is playing.
class event_check_music_db_err : public event  {
public:
  // Constructor
  event_check_music_db_err(player & pplayer) : Player(pplayer) {};
  // Destructor
  virtual ~event_check_music_db_err() {}; // Remove annoying "virtual functions but non-virtual destructor" warnings
  // Run the event.
  virtual void run();
private:
  player & Player; // A reference to the main player object
};

#endif
