/***************************************************************************
                          player_logger.h  -  description
                             -------------------
    begin                : Tue Sep 9 2003
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

#ifndef PLAYER_LOGGER_H
#define PLAYER_LOGGER_H
/**
  *@author David Purdy
  */

#include "logger.h"
#include <string>
#include "rr_psql.h"

class player_logger : public logger {
public: 
  player_logger(pg_connection & conn) : DB(conn) {}; // initialize with database connection object.
  virtual void handle_log();
private:
  pg_connection & DB; // Database connection object

  // Player-specific error logging functions...
  void player_LogError(const string strType, const string strMsg, const string strFrom);
  void player_LogMessage(const string strType, const string strMsg, const string strFrom); 
};

#endif
