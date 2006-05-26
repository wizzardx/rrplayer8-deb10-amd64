
// Header defining misc utility player functions

#ifndef PLAYER_UTIL_H
#define PLAYER_UTIL_H

#include <string>

// Forward declarations:
class pg_connection;

void write_liveinfo_setting(pg_connection & db, const std::string & strname, const std::string & strvalue);

#endif
