
#include "player_util.h"
#include "common/psql.h"

void write_liveinfo_setting(pg_connection & db, const string & strname, const string & strvalue) {
  // There is a tblLiveInfo table in the DB that stores the same information as
  // the liveinfo.chk file. This procedure is used to update one of these settings
  string strSQL;

  strSQL = "SELECT lngStatus FROM tblliveInfo WHERE strstatusname = " + psql_str(strname);
  pg_result rs = db.exec(strSQL);
  // Generate part of the SQL string - if strValue is empty then a NULL value
  // must be written.
  if (!rs)
    // A new status setting - INSERT
    strSQL = "INSERT INTO tblliveinfo (strstatusname, strstatusvalue) VALUES (" + psql_str(strname) + ", " + psql_str(strvalue) + ")";
  else
    // An existing one - UPDATE
    strSQL = "UPDATE tblliveinfo SET strstatusvalue = " + psql_str(strvalue) + " WHERE strstatusname = " + psql_str(strname);

  db.exec(strSQL);
}

