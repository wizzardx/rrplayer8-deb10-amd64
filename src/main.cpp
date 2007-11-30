/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Wed Mar 16 18:10:44 GMT 2005
    copyright            : (C) 2005 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#include "player.h"
#include "common/exception.h"
#include "common/rr_misc.h"
#include <iostream>

// A "call-back" logging function:
void log(const log_info & LI) {
  // Standard log handling - logfile & clog
  rr_log_file(LI, PLAYER_LOG_FILE, PLAYER_DEBUG_LOG_FILE);
}

int main(int argc, char *argv[])
{
  try {
    // Setup the logger:
    logging.add_logger(log);

    // Check arguments:
    bool blndebug = false; // Set to true for Player debugging messages.
    {
      if (argc > 1) {
        string strarg = argv[1];
        if (strarg == "help") {
          cout << endl;
          cout << "Run the Player with one of these arguments:" << endl;
          cout << endl;
          cout << "  <No arguments>  - Start up a normal Player session" << endl;
          cout << "  debug           - Start up the Player in debug mode." << endl;
          cout << "  help            - Display this help message." << endl;
          cout << endl;
          return EXIT_SUCCESS;
        } else if (strarg == "debug") {
          log_line("Starting the Player in debug mode.");
          logging.blndebug = true;
        } else my_throw("Unknown argument: " + strarg);
      }
    }

    //  Init the player:
    player player;

    // Run the Player main event loop:
    player.run();

    return EXIT_SUCCESS;
  } catch_exceptions;
  cout << "Type 'player help' for more information." << endl;
  return EXIT_FAILURE;
}
