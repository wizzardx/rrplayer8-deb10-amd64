/***************************************************************************
                          player_logger.cpp  -  description
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

#include "player_logger.h"
#include "rr_utils.h"

void player_logger::handle_log() {
  // Do the player-specific database logging
  string strPriority="";
  switch (log_info.LogType) {
    case LT_MESSAGE: {
      // Use the player's "log" table.
      player_LogMessage("LOW", log_info.strMessage, log_info.strSourceFunction);
    } break; // end case
    case LT_WARNING: case LT_ERROR:{
      // Player currently uses the same output table for errors and warnings...
      player_LogError("MEDIUM", log_info.strMessage, log_info.strSourceFunction);
    } break; // end case
    default: {
      // Unknown log type! Log it as an error, then make another log for unknown type...
      // Log as an error..
      // Log a new error
      player_LogError("MEDIUM", "Unknown log type", __FUNCTION__);
      throw(1);
    } // end default
  } // end switch
}

/***********************************************************
    Log important messages to the database.
***********************************************************/
void player_logger::player_LogError(const string strType, const string strMsg, const string strFrom) {
  // Write the error message to the database and to the frontend

  if (DB.ok()) {
    // Build a query to see whether an error record must be updated or added
    string strSQL = "SELECT lngErrorNumber, lngErrorOccurred FROM tblErrors"
                             " WHERE strMsg = " + psql_str(strMsg) + " AND dtmDate = " + psql_date +
                             " AND strType = " + psql_str(strType) + " AND strFrom = " + psql_str(strFrom);

    pg_result RS = DB.exec(strSQL);

    if (RS.recordcount() == 0) {
      // No matching records found, create a new one
      strSQL =
        "INSERT INTO tblErrors (dtmDate, dtmTime, strType, strMsg, strFrom, lngErrorOccurred) "
        "VALUES (" + psql_date + "," + psql_time + "," + psql_str(strType) + "," + psql_str(strMsg) + "," + psql_str(strFrom) + ",1)";
      DB.exec(strSQL);
    }
    else {
      // This error has already occured today, update the time and occurances
      int lngErrorNumber = strtoi(RS.field("lngErrorNumber", "-1"));
      int lngOccurred = strtoi(RS.field("lngErrorOccurred", "-1"));

      strSQL = "UPDATE tblErrors SET dtmTime = " + psql_time +
          ", lngErrorOccurred = " + itostr(lngOccurred + 1) +
          " WHERE lngErrorNumber = " + itostr(lngErrorNumber);
      DB.exec(strSQL);
    }
  }
  else {
    // Write a message to the log file and the standard out...    
    log_line("Unable to log to database - bad connection.");
  }  
}

void player_logger::player_LogMessage(const string strType, const string strMsg, const string strFrom) {
  // Writes a log to the database. (tblLog)
  if (DB.ok()) {
    // Delete previous logs of the exact same description, so that the recent logs always have the
    // highest primary key... (to aid comprehension of the table contents...)
    string strSQL = "DELETE FROM tblLog WHERE tblLog.strMsg = " + psql_str(strMsg);
    DB.exec(strSQL);

    // Now insert the line (it will be the most recent record...)
    strSQL = "INSERT INTO tblLog (dtmDate, dtmTime, strType, strMsg, strFrom) VALUES (" +
           psql_date + ", " + psql_time + " ," + psql_str(strType) + """, " + psql_str(strMsg) + ", " + psql_str(strFrom) + ")";
    DB.exec(strSQL);
  }
  else {
    // Write a message to the log file and the standard out...
    log_line("Unable to log to database - bad connection.");
  }
}
