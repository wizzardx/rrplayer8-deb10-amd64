
#include "rr_misc_db.h"

#include "my_string.h"
#include "exception.h"
#include "rr_misc.h"

using namespace std;

static string strdefs_table="tbldefs"; // Used by load_tbldefs and save_tbldefs

string load_tbldefs(pg_conn_exec & db, const string & strsetting, const string & strdefault, const string & strtype, const string & strdesc) {
  // Load a value of a specific setting from the database
  // Simplified version (from VB) - load the setting from the table, don't check the type
  pg_result rs = db.exec("SELECT strdatatype, strdef_val, strdef_descr FROM " + strdefs_table + " WHERE strdef = " + psql_str(strsetting));
  if (rs.empty()) {
    // The setting was not found in the database, add it there, and return
    // the default setting value to the caller
    // - But don't save empty values to the database!
    if (strdefault != "") {
      log_line("Writing new setting " + strsetting + " to the database (" + strdefs_table + ").");
      save_tbldefs(db, strsetting, strtype, strdefault, strdesc);
    }
    return strdefault;
  } else {
    // The setting was found in the database - check it's type and then load
    // it or use the default value if the entry was incorrect
    string strdb_type  = rs.field("strdatatype", ""); // Data type of the setting read in from the database
    string strdb_value = rs.field("strdef_val", "");  // Value of the setting read in from the database
    string strdb_desc  = rs.field("strdef_descr", "");

    // If the description has changed, then save it:
    if (strdesc != "" && strdesc != strdb_desc) {
      db.exec("UPDATE " + strdefs_table + " SET strdef_descr = " + psql_str(strdesc) + " WHERE strdef = " + psql_str(strsetting));
    }

    string strret = ""; // What gets returned.
    try {
      if (strtype == "byt") {
        int inttemp=strtoi(strdb_value);
        if (inttemp < 0 || inttemp > 255) {
          my_throw(itostr(inttemp) + " is not between 0 and 255.");
        }
      }
      else if (strtype == "int") {
        strtoi(strdb_value);
      }
      else if (strtype == "lng") {
        strtol(strdb_value);
      }
      else if (strtype == "bln") {
        strtobool(strdb_value);
      }
      else if (strtype == "str") {
        // Do nothing
      }
      else if (strtype == "dtm") {
        parse_datetime_string(strdb_value);
      }
      else if (strtype == "flt") {
        strtod(strdb_value);
      }
      else LOGIC_ERROR; // Unknown type!

      // Execution to this point means there was no problem parsing the setting.
      strret = strdb_value;
    }
    catch(exception & e) {
      log_warning("Setting " + strsetting + "(" + strtype + "), has a problem: " + e.what() + " Changing from '" + strdb_value + "' to the default value \"" + strdefault + "\"");
      save_tbldefs(db, strsetting, strtype, strdefault, strdesc);
      strret=strdefault;
    }

    return strret;
  }
}

void save_tbldefs(pg_conn_exec & db, const string & strsetting, const string & strtype, const string & strvalue, const string & strdesc) {
  // Simplified version (from VB) - save the setting to the table as a string, but don't check the type
  string strsql = "SELECT lngdef FROM " + strdefs_table + " WHERE strdef = " + psql_str(strsetting);
  pg_result rs = db.exec(strsql);

  if (!rs.empty()) {
    // The setting already exists in the database, update it
    strsql = "UPDATE " + strdefs_table + " SET strdatatype = " + psql_str(strtype) + ", strdef_val = " + psql_str(strvalue);
    if (strdesc != "") {
      strsql += ", strdef_descr = " + psql_str(strdesc);
    }
    strsql += " WHERE strdef = " + psql_str(strsetting);
  }
  else {
    // The setting was not found, add it to the database
    strsql = "INSERT INTO " + strdefs_table + " (strdef, strdatatype, strdef_val";
    if (strdesc != "") {
      strsql += ", strdef_descr";
    }
    strsql += ") VALUES (" + psql_str(strsetting) + ", " + psql_str(strtype) + ", " + psql_str(strvalue);
    if (strdesc != "") {
      strsql += ", " + psql_str(strdesc);
    }
    strsql += ")";
  }
  db.exec(strsql);
}

void set_tbldefs_table(const string & strtable) { // eg: Use tblschedmon_defs instead of tbldefs;
  strdefs_table = strtable;
}

long get_lng_from_db_func(const string & strindex, pg_conn_exec & T, const string & strsql_select, const string & strsql_insert, const string & strsql_update) {
  pg_result rs = T.exec(strsql_select);
  int intrecords = rs.size();
  if (intrecords > 1) my_throw(itostr(intrecords) + " records returned by the SELECT query!. SQL: " + strsql_select);
  else if (intrecords == 0) {
    T.exec(strsql_insert);
    rs = T.exec(strsql_select);
  }
  else if (intrecords == 1) {
    pg_result rstest = T.exec(strsql_update);
    if (rstest.affected_rows() != 1) my_throw("Bad UPDATE query! " + itostr(rstest.affected_rows()) + " rows updated instead of 1. SQL: " + strsql_update);
  }
  else LOGIC_ERROR;
  return strtol(rs.field(strindex));
}

