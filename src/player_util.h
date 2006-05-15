
// Header defining misc utility player functions

#ifndef PLAYER_UTIL_H
#define PLAYER_UTIL_H

#include "common/psql.h"

void write_liveinfo_setting(pg_connection & db, const string & strname, const string & strvalue);

#endif
