/***************************************************************************
                          timed_event.h  -  description
                             -------------------
    version              : v0.05
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

#ifndef EVENT_H
#define EVENT_H
#define EVENT_H_VERSION 5 // Meaning 0.05

#include "check_library_versions.h" // Always last: Check the versions of included libraries.

/**
  *@author David Purdy
  */

// Client programs should make subclasses of this class, and then pass them to "timed_events"

class event {
public:
  virtual void run()=0;
};

#endif
