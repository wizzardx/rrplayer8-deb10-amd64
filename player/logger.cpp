/***************************************************************************
                          logger.cpp  -  description
                             -------------------
    version              : 0.08
    begin                : Thu Sep 11 2003
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

#include "logger.h"
#include "rr_utils.h"
#include "check_library_versions.h"

using namespace rr;

// Constructor
logger::logger() {
  // Reset the data

  // Log information:
  log_info.strMessage="";
  log_info.LogType=LT_MESSAGE;
  log_info.strProg="";
  log_info.strSourceFile="";
  log_info.strSourceFunction="";
  log_info.lngSourceLine=0;

  // Set formatting strings:
  strDateFormat = "%dd/%mm/%yyyy";
  strTimeFormat = "%HH:%NN:%SS";
  strLogFormat = "[%DATE %TIME] %FUNCTION(): %TYPE_NOTDEFAULT%MESSAGE%SOURCE_ERROR";
}

// This function is called by an outside processes when there is a log to be handled is picked up.
void logger::set_log(const string & strMessage_,    // eg: "Could not find the config file! Terminating"
                                const log_type LogType_,            // eg: LT_ERROR
                                const string & strProg_,           // eg: "player"
                                const string & strSourceFile_, // eg: "player.cpp"
                                const string & strSourceFunction_, // eg: "player_init_func"
                                const long lngSourceLine_) {   // eg: 241

  // Copy the variables accross...
  log_info.strMessage = strMessage_;
  log_info.LogType = LogType_;
  log_info.strProg = strProg_;

  log_info.strSourceFile = strSourceFile_;
  log_info.strSourceFunction = strSourceFunction_;
  log_info.lngSourceLine = lngSourceLine_;
}

// A function for fetching a line to be displayed, based on the log information.
string logger::GetLogDescr(const string & strFormat) {
   // GetLogDescr takes the individual elements of a given error log, and returns a single string describing them all
  // - useful for descendant classes which want a single line to log, not the individual elements.
  //   - eg cout, logfile
  string strRet = strFormat;
  strRet=replace(strRet, "%DEFAULT", strLogFormat); // %DEFAULT is for when the user does not specify a format.
  strRet=replace(strRet, "%SOURCE_ERROR", ((log_info.LogType == LT_ERROR) ? " (%FILE:%LINE)": ""));    
  strRet=replace(strRet, "%DATE", format_datetime(Now(), strDateFormat));
  strRet=replace(strRet, "%TIME", format_datetime(Now(), strTimeFormat));
  strRet=replace(strRet, "%PROGRAM", log_info.strProg);
  strRet=replace(strRet, "%FILE", log_info.strSourceFile);
  strRet=replace(strRet, "%FUNCTION", log_info.strSourceFunction);
  strRet=replace(strRet, "%LINE", itostr(log_info.lngSourceLine));
  strRet=replace(strRet, "%TYPE_NOTDEFAULT", ((log_info.LogType != default_log_type) ? "%TYPE: ": ""));
  strRet=replace(strRet, "%TYPE", GetLogTypeDescr());
  strRet=replace(strRet, "%MESSAGE", log_info.strMessage);
 
  // Now return the line we've built up:
  return strRet;
}

 // Functions to get descriptions of the enumerations... LogType, LogPriority...

string logger::GetLogTypeDescr() {
  switch (log_info.LogType) {
    case LT_MESSAGE: {
      return "MESSAGE";
    } break;
    case LT_WARNING: {
      return "WARNING";
    } break;
    case LT_ERROR: {
      return "ERROR";
    } break;
    default: {
      return "Error: Unknown Log Type!";
    }
  }
}
