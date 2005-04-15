/***************************************************************************
                          timed_events.cpp  -  description
                             -------------------
    begin                : Mon Sep 29 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#include "timed_events.h"
#include "exception.h"
#include "logging.h"
#include "string.h"

// Constructor
timed_events::timed_events() {
  // Reset object attributes...
  blnevents_enabled = true; // Global enabled/disabled status.
  dtmnow = dtmprev_now = datetime_error; // System clock error checking..
}

// Adding and removing events:
void timed_events::add_event(event & Event, const long lngSecondInterval, bool blnEnabled) {
  // The default value for blnenabled is TRUE
  // - An event can only be enabled if the Event is not already listed, and if lngSecondInterval is positive

  if (event_listed(Event)) {
    my_throw("Cannot add an event more than once!");    
  }

  if (lngSecondInterval <= 0) {
    // Cannot set a negative or 0 interval!
    my_throw("Cannot add an event with a non-positive interval! (" + itostr(lngSecondInterval) + ")");
  }
   
  // The interval is positive - so add it to the list.
  event_timing_info EventInfo; // - Declare a structure
  EventInfo.Event = &Event; // - Populate the structure
  EventInfo.blnEnabled = blnEnabled;
  EventInfo.lngInterval = lngSecondInterval;

  EventInfo.dtmNextRun = now() + lngSecondInterval;
  EventTiming.push_back(EventInfo); // Copy it to a new record in the vector.
}

void timed_events::remove_event(const event & Event) {
  // Find the record...
  EventTiming_iterator I = find_event(Event);

  // Was the scan successful?

  if (I == EventTiming.end()) {
    my_throw("Event object not found! Cannot remove it!");    
  }
 
  // Remove the Event from the list.
  EventTiming.erase(I);
}

// Enabling and disabling events
bool timed_events::event_enabled(const event & Event) {
  bool blnResult = false;
  EventTiming_iterator I = find_event(Event);
  if (I != EventTiming.end()) {
    blnResult = I->blnEnabled;
  } // end if
  return blnResult;
}

void timed_events::enable_event(const event & Event) {
  EventTiming_iterator I = find_event(Event);
  if (I != EventTiming.end()) {
    I->blnEnabled = true;
  } // end if
  else {
    // Event not found!
    my_throw("Event not listed, it cannot be enabled!");
  }
}

void timed_events::disable_event(const event & Event) {
  EventTiming_iterator I = find_event(Event);
  if (I != EventTiming.end()) {
    I->blnEnabled = false;
  } // end if
  else {
    // Event not found!
    my_throw("Event not listed, it cannot be disabled!");
  }
}

// Global event disable and enable:
bool timed_events::events_enabled() {
  return blnevents_enabled;
}

void timed_events::enable_events() {
  blnevents_enabled = true;
}

void timed_events::disable_events() {
  blnevents_enabled = false;
}

// Changing the interval
void timed_events::set_event_interval(const event & Event, const long lngSecondInterval) {
  EventTiming_iterator I = find_event(Event);
  if (I != EventTiming.end()) {
    // Is the interval a positive integer?
    if (lngSecondInterval > 0) {    
      I->lngInterval = false;
    }
    else {
      // The interval is negative!
      my_throw("Cannot set an event's interval to a non-positive number of seconds! (" + itostr(lngSecondInterval)+ ")");
    }
  } // end if
  else {
    // Event not found!
    my_throw("Event not listed, it's interval cannot be set! (to " + itostr(lngSecondInterval) + ")");
  }
}

// Check which events must be run now, and run them.
void timed_events::run_events() {
  // Fetch the present time
  dtmnow = now();

  // Are any events allowed to run now? (ie, have events been disabled?
  if (blnevents_enabled) {
    // Event running is enabled. Do some error checking and then see if it is time to run any events.
    // - Check the system time
    system_time_error_check();
    // - Check for events to run now:

    EventTiming_iterator I = EventTiming.begin();
    while (I != EventTiming.end()) {
      // Is the event enabled?
      if (I->blnEnabled) {
        // Check that the time until the next event occurance, is not too far in the future.
        if (I->dtmNextRun - dtmnow > I->lngInterval) {
          I->dtmNextRun = dtmnow + I->lngInterval;
          log_message("A timed event was resynchronized (was the system time moved backwards?)");
        }

        // Has the time for the event come?
        if (dtmnow >= I-> dtmNextRun) {
          // Time to run the event!
          if (I->Event != NULL) {
            // The event object pointer is set.
            try {
              I->Event->run();
            } catch_exceptions; // This is so that our event-scheduling logic is not disrupted.
              
            // Now advance the time of the next run:
            I->dtmNextRun = dtmnow + I->lngInterval;
          }
          else {
            // The event object pointer is not set!
            my_throw("Event object is set to NULL!");
          }
        }
      }
      // Advance to the next event.
      I++;
    } // end while
  }
}

// Reset the event timing - ie, all events will run at the next call to run_events() and not wait for their interval to elapse..
void timed_events::reset_timing() {
  // Run through the list of events and reset the "next time to run" to now plus their intervals.
  dtmnow = now();
  EventTiming_iterator I = EventTiming.begin();
  while (I != EventTiming.end()) {
    I->dtmNextRun = dtmnow + I->lngInterval;
    ++I;
  } // end while
}

// Search for an event..
timed_events::EventTiming_iterator timed_events::find_event(const event & Event) {
  EventTiming_iterator I = EventTiming.begin();
  bool blnFound = false;
  while (!blnFound && (I != EventTiming.end())) {
    if (I->Event == &Event) {
      // We found the event - don't progress to the next one.
      blnFound = true;
    }
    else {
      // Event not found, so progress to the next one.
      ++I;
    }
  }
  return I; // I will be EventTiming.end() if the event could not be found, otherwise it will be an iterator pointing to the
               // correct element in the vector.       
}

// Is a given event already in the event list?
bool timed_events::event_listed(const event & Event) {
  bool blnListed = false;      // Set to true if the event is found.
  try {
    blnListed = (find_event(Event) != EventTiming.end());
  } catch_exceptions;
  return blnListed;
}

// Check the system clock, so that logic errors do not occur when the system time shifted backwards.
void timed_events::system_time_error_check() {
  try {
    if (dtmnow < dtmprev_now) {
      reset_timing();
      log_warning("The system time has been changed! Timed events recallibrated.");
    } // end if
  } catch_exceptions;
  dtmprev_now = dtmnow;
}
