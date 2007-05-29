/// @file
/// A collection of miscellenious Radio Retail-specific logic.
/// Use this for useful RR logic which hasn't been sorted yet.
/// Logic inside this library will eventually be moved into other rr_ libraries.
/// - If you use this library, be prepared to update your includes in the future.

#ifndef RR_MISC_DB_H
#define RR_MISC_DB_H

#include "logging.h"
#include "psql.h"
#include <string>

using namespace std;

// Manipulate tbldefs:
string load_tbldefs(pg_conn_exec & db, const string & strsetting, const string & strdefault, const string & strtype, const string & strdesc = "");
void save_tbldefs(pg_conn_exec & db, const string & strsetting, const string & strtype, const string & strvalue, const string & strdesc = "");
void set_tbldefs_table(const string & strtable); // eg: Use tblschedmon_defs instead of tbldefs;

// SQL logic shortcuts:

// Define a macro & function for running queries to create or update a single record, and fetch the index:
#define get_lng_from_db(strindex) get_lng_from_db_func(strindex, T, strsql_select, strsql_insert, strsql_update)
long get_lng_from_db_func(const string & strindex, pg_conn_exec & T, const string & strsql_select, const string & strsql_insert, const string & strsql_update);

#endif
