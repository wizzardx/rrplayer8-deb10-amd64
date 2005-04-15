/***************************************************************************
                          event.h  -  description
                             -------------------
    begin                : Mon Sep 29 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifndef EVENT_H
#define EVENT_H

/**
  *@author David Purdy
  */

// Client programs should make subclasses of this class, and then pass them to "timed_events"

class event {
public:
  virtual void run()=0;
};

#endif
