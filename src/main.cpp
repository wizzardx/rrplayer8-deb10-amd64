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

// A "call-back" logging function:
void log(const log_info & LI) {
  // Standard log handling - logfile & cout.
  // Doing this here instead of in player::log in case the player object no longer
  // exists (eg: Player constructor threw an execption)
  if (pplayer != NULL) {
    pplayer->log(LI); // Log to cout, file & db.
  }
  else rr_log_file(LI, PLAYER_LOG_FILE); // Just log to cout & file.
}

int main()
{
  try {
    // Setup the logger:
    logging.add_logger(log);

    //  Init and run the player:
    player player;
    player.run();

    return EXIT_SUCCESS;
  } catch_exceptions;
  return EXIT_FAILURE;
}
