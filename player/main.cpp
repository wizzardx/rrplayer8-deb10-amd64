/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Mon May 13 09:32:07 SAST 2002
    copyright            : (C) 2002 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <stdlib.h>
#include <unistd.h>

// Header files used by the main app
#include "player.h"

//int main(int argc, char *argv[]) {
int main() {
  try {
    player Player;
    if (Player) {
      // Start
      while (Player.do_events()) { // do_events() returns false when the player must quit.
        // Check for player events to be run, then wait 1 second...
        sleep(10);
      } // end while
      return EXIT_SUCCESS;
    } // end if
    else return EXIT_FAILURE;
  } catch_exceptions;
} // end function
