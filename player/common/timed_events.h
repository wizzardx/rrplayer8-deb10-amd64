/***************************************************************************
                          timed_events.h  -  description
                             -------------------
    begin                : Mon Sep 29 2003
    copyright            : (C) 2003 by David Purdy
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

#ifndef TIMED_EVENTS_H
#define TIMED_EVENTS_H

/**
  *@author David Purdy
  */

#include <string>
#include <vector>
#include "event.h" // The events to be run by the timed_events class.
#include "time.h"

class timed_events {
public:
  // Constructor

  timed_events();

  // Adding and removing events:
  void add_event(event & Event, const long lngSecondInterval, bool blnEnabled=true);
  void remove_event(const event & Event);

  // Enabling and disabling events
  bool event_enabled(const event & Event);
  void enable_event(const event & Event);
  void disable_event(const event & Event);

  // Global event disable and enable:
  bool events_enabled();
  void enable_events();
  void disable_events();

  // Changing the interval
  void set_event_interval(const event & Event, const long lngSecondInterval);  

  // Check which events must be run now, and run them.
  void run_events();

  // Reset the event timing - ie, all events "next time to run" will be reset to NOW + their interval.
  void reset_timing();  
  
private:
  bool blnevents_enabled; // Global enabled/disabled setting - can any events run?

  // A structure for storing information about event timing:
  struct event_timing_info {
    event * Event; // A (polymorphic) pointer to the event to be run.
    bool blnEnabled; // Is the event allowed to be run?
    long lngInterval; // The minimum number of seconds to elapse between each call to this event.
    datetime dtmNextRun; // Time when the event must next be run.
  };

  // A vector to store timing information for all the events:
  vector <event_timing_info> EventTiming;

  // An iterator type:
  typedef vector <event_timing_info>::iterator EventTiming_iterator;

  // Search for an event..
  EventTiming_iterator find_event(const event & Event);  

  // Is a given event already in the event list?
  bool event_listed(const event & Event);

  // Check the system clock, so that logic errors do not occur when the system time shifted backwards.
  void system_time_error_check();
  datetime dtmnow, dtmprev_now;
};

#endif
