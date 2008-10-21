
#ifdef __linux__ // Postgresql access is only possible under linux at this time!

#include "psql.h"
#include "exception.h"
#include "my_string.h"
#include "logging.h"

#include <pqxx/transaction>
#include <pqxx/nontransaction>

/********************************************************************************************
          A Connection wrapper
********************************************************************************************/

// Generate a connection string from parts;

string pg_create_conn_str(const string & strhost, const string & strport, const string strdbname, const string strusername, const string strpassword) {
  return "host=" + strhost + " port=" + strport + " dbname=" + strdbname + " user=" + strusername + " password=" + strpassword;
}

// Constructor
pg_connection::pg_connection() {
  strconn = "";
  pconn = NULL;
  ptransaction = NULL;
  intmax_retries = 0; // No retries by default.
  intretry_interval = 30;
  client_conn_err_code = NULL; // Event code to call when a connection error is found.
  lngnum_exec_calls = 0; // Number of exec calls made through the connection.
  blnauto_commit=true;
}

// Destructor
pg_connection::~pg_connection() {
  if (ptransaction != NULL) {
    delete ptransaction;
    ptransaction = NULL;
  }
  if (pconn != NULL) {
    delete pconn;
    pconn = NULL;
  }
}

// Set the connection retry settings used by open() and check()
void pg_connection::set_connect_retries(const int intretries) {
  intmax_retries  = intretries;
}

void pg_connection::set_connect_retry_interval(const int intseconds) {
  intretry_interval = intseconds;
}

// Open a connection, keep retrying until successful or we've retried enough.
void pg_connection::open(const string & strConn) {
  if (strConn != "") strconn = strConn;

  // Check: pconn must be NULL.
  if (pconn != NULL) {
    my_throw("Connection is already open! Call close() first.");
  }

  // Check: strconn must not be empty.
  if (strconn == "") {
    my_throw("Connection string is empty.");
  }

  establish_connection();
}

// Close a connection.
void pg_connection::close() {
  // Check: pconn must not be NULL.
  if (pconn == NULL) {
    my_throw("connection not open!");
  }

  // First delete the transaction pointer if it is not NULL...
  if (ptransaction != NULL) {
    delete ptransaction;
    ptransaction = NULL;
  }

  // Delete the connection pointer and set it to NULL.
  delete pconn;
  pconn = NULL;
}

// Check a connection - ie: if the connection is bad then attempt to reconnect. Return true
// if the connection is alive at the end of the function
void pg_connection::check() {
  // Check: Has strconn been set yet?
  if (strconn == "") my_throw("Connection string not set, unable to check database connection!");
  establish_connection();
}

// Is the connection open? This method does not actively check the connection or attempt to reconnect.
bool pg_connection::isopen() {
  bool blnret = false;
  if (pconn != NULL) {
    blnret = pconn->is_open();
  }
  return blnret;
}

// Execute a query and return a result:
pg_result pg_connection::exec(const string & strsql) { // An exception is thrown if there is a SQL execution error
  lngnum_exec_calls++; // Increment the counter

  // If the number of calls to this function (in this object) is a multiple of 100,
  // then close and open the database connection. This is because the postgresql
  // server memory usage of a single connection can grow over time and use up the entire
  // machine's memory! (When the server is badly configured, as is the case with .51)

  // Only close and open the connection if we're running in autocommit mode:
  if ((lngnum_exec_calls % 100 == 0) && blnauto_commit) {
    close();
    establish_connection();
  }

  // If the connection string is defined, but the connection is down, this is possibly
  // because the application is attempting to run with the connection down most of
  // the time (possibly due to bad database service config), but forgot to reopen
  // the connection before running a query. We open the connection here in that case,
  // but don't close it - let the app do that itself later.
  if (!isopen() && strconn != "") {
    establish_connection();
    log_warning("Application probably forgot to re-open it's database connection.");
  }

  // Check the transaction object.
  if (ptransaction == NULL) my_throw("No database connection!");

  // Attempt to execute the query
  try {
    pg_result RS(ptransaction->exec(strsql));
    RS.strsql=strsql; // Also store the SQL that generated the recordset...
    return RS;
  } catch (const exception & e) {
    // Fetch the error, tidy it up and remove "ERROR: " from the start.
    string strError = e.what();
    if (substr(strError, 0, 6) == "ERROR:") {
      strError = substr(strError, 6);
    }
    strError = trim(strError);
    // Do some more formatting.
    strError = replace(strError, "\n", "");
    strError = replace(strError, "\t", " ");

    // If there was a fatal error executing the query, then check the connection. If there is a connection
    // error then this will disconnect and reconnect at intervals until the connection is successful.

    if (ucase(substr(strError, 0, 5)) == "FATAL") {
      // There was a fatal execution error. This usually means some sort of problem connecting to the postgres server...
      log_line("FATAL query execution error. Checking the database connection...");
      check();
    }

    // Now throw an exception describing the query error:
    my_throw("SQL query failed. The query: \"" + strsql + "\". The error: \"" + strError + "\"");
  }
}

pg_result pg_connection::exec(const string & strsql,
                              const pg_params & params) {
  return exec(format_string_with_vector(strsql, params, "?"));
}

void pg_connection::call_on_connect_error(void(*func)()) {
  // Set a pointer to a callback function to be called if a connection error occurs...
  client_conn_err_code = func;
}

// Function used internally by open() and check():
void pg_connection::establish_connection() {
  bool blnConnected = false; // Gets set to true if the connection is fine.

  // There are 2 uses for establish_connection:
  //   1) Check an existing connection (with retries), and
  //   2) Create a new connection (with retries)
  if (strconn == "") {
    my_throw("Connection string not set!");
  }

  // Loop and attempt to connect....
  bool blnloop_done = false; // Set to true when the retry loop ends.
  long lngattempt_num = 0; // Attempt once, and then the retries are the attempts after.
  while (!blnloop_done) {
    ++lngattempt_num;
    string strconn_err = "";  // Describes the database problem that occured..
    try {
      // If the connection object pointer is not set, then attempt to create a new connection...
      if (pconn == NULL) {
        pconn = new pqxx::connection(strconn);
      }
      // If the transaction object pointer is not set, then attempt to create a new transaction...
      if (ptransaction == NULL) {
        if (blnauto_commit) {
          // Queries will be written directly to the backend
          ptransaction = new pqxx::nontransaction(*pconn);
        }
        else {
          // You have to run a "commit" to send changes to the backend.
          ptransaction = new pqxx::work(*pconn);
        }
      }

      // Attempt to query a system table...
      ptransaction->exec("SELECT datname FROM pg_database LIMIT 1");

      // If there were no exceptions, then the connection was successful.
      blnConnected = true;
    }
    catch(const PGSTD::exception &e) {
      // Connection attempt did not succeed.
      // - Fetch the error description
      strconn_err = trim(e.what());
      // Do some more formatting.
      strconn_err = replace(strconn_err, "\n", "");
      strconn_err = replace(strconn_err, "\t", " ");
    }
    catch (...) {
      // All other exceptions
      strconn_err = "Unexpected error!";
    }

    // Was the connection/check successful?
    if (blnConnected) {
      // Allow the loop to end.
      blnloop_done = true;
      // Log a message if the connection succeded after the first attempt (ie if the logic went into "retry" mode)
      if (lngattempt_num > 1) {
        log_message("Database connection success.");
      }
    }
    else {
      // Connection/Check failed.
      // Destroy the transaction and connection record if they are set...

      if (ptransaction != NULL) {
        delete ptransaction;
        ptransaction = NULL;
      }

      if (pconn != NULL) {
        delete pconn;
        pconn = NULL;
      }

      // Display the error:
      log_error("Connection error: " + strconn_err);

      // Now call some client code (if specified), because there was an error...
      if (client_conn_err_code != NULL) {
        log_message("Connection error occured, running client's callback code...");
        try {
          client_conn_err_code(); // in the case of the player, this assures that music playback does take place.
        } catch_exceptions; // Just in case the client callback code throws an exception.
      }

      // Prepare for a retry. Do connection attempts remain?
      if ((lngattempt_num <= intmax_retries) || (intmax_retries == PG_RETRY_INFINITE)) {
        // We haven't reached the attmpts limit yet.
        // Display a message asserting that retries will continue.
        if (lngattempt_num == 1) {
          log_message("I will attempt to connect to the database every " + itostr(intretry_interval) + " seconds.");
        }
        // Sleep for the specified # of seconds...
        sleep(intretry_interval);
        // Now display that we are attempting to make a connection (only shown for retries)
        log_message("Database connection attempt #" + ltostr(lngattempt_num + 1) + "...");
      } // end if
      else {
        // We cannot make any more connection attempts.
        blnloop_done = true;
        my_throw("Could not establish a connection to the database.");
      } // end else
    }
  }
}

// Call this function to switch between a NonTransaction and a Transaction:
void pg_connection::set_auto_commit_mode(const bool autocommit) {
  // A different autocommit setting?
  if (autocommit != blnauto_commit) {
    blnauto_commit = autocommit;

    // Is there an open transaction?
    if (ptransaction != NULL) {
      // Yes: So delete the transaction & create a new one of the correct type
      delete ptransaction;
      ptransaction = NULL;

      if (blnauto_commit) {
        // Queries will be written directly to the backend
        ptransaction = new pqxx::nontransaction(*pconn);
      }
      else {
        // You have to run a "commit" to send changes to the backend.
        ptransaction = new pqxx::work(*pconn);
      }
    }
  }
}

/********************************************************************************************************
          A Result wrapper
*********************************************************************************************************/
// Copy constructor
pg_result::pg_result(const pg_result & pg_res) {
  presult = NULL;
  introw_num = 1;
  presult = new pqxx::result(*(pg_res.presult));
  strsql = pg_res.strsql;
}

// Assignment operator
pg_result& pg_result::operator=(const pg_result & pg_res) {
  // Check for self-assignment...
  if (this != &pg_res) {
    // Is the result pointer set?
    if (presult != NULL) {
      delete presult;
      presult = NULL;
    }

    // Now create a new Result pointer...
    introw_num = 1;
    presult = new pqxx::result(*(pg_res.presult));
    strsql=pg_res.strsql;
  } // end self-assignment check.

  // This allows assignments to be chained (ie: a = b = c = d)

  return *this;
}

// Destructor
pg_result::~pg_result() {
  // Is the result pointer set?
  if (presult != NULL) {
    delete presult;
    presult = NULL;
  }
}

// Field retrieval
string pg_result::field(const string & strfield_name, const char * strdefault_val) const {
  // Rethrows exceptions. The re-thrown exceptions also get this file, function, and a nearby line.
  try {
    const pqxx::result::field & field = (*presult).at(introw_num-1).at(strfield_name);
    if ((field.is_null()) || (field.c_str() == NULL)) {
      if (strdefault_val == NULL) {
        // The field is NULL and there is no alternate value to return!
        // Either 1) NULL data values are not allowed.
        //        2) NULL values are allowed, but the caller of pg_result::field forgot to specify an alternate value
        my_throw("Field \"" + strfield_name + "\" is NULL (and you did not specify a \"default\" value). Please check the data! Record #" + itostr(introw_num) + ", returned from query: " + strsql);
      }
      return strdefault_val;
    }
    else {
      return field.c_str();
    }
  }
  catch (const my_exception &e) {
    throw; // Re-throw my_exceptions...
  }
  catch (const exception &e) {
    my_throw(e.what());
  }
}

bool pg_result::field_is_null(const string & strfield) const {
  // Rethrows exceptions
  try {
    const pqxx::result::field & field = (*presult).at(introw_num-1).at(strfield);
    return field.is_null();
  }
  catch (const exception &e) {
    my_throw(e.what());
  }
}

void pg_result::operator ++(int) { // Move to the next record
  if (this) { // Any records left?
    ++introw_num;
  }
  else my_throw("No more records left!");
}

// A constructor that can only be called by friend class pg_transaction
pg_result::pg_result(const pqxx::result res) {
  introw_num = 1;
  presult = new pqxx::result(res);
}

// This is basically a Connection wrapper. When you create it, it creates a new database
// connection, but setup in Transaction (not NonTransaction) mode.

// Constructor:
pg_transaction::pg_transaction(pg_connection & conn) {
  // The object already has a connection object. Disable "autocommit" mode, and
  // open the connection.
  connection.set_auto_commit_mode(false);
  connection.open(conn.strconn);

  // And variables to store whether the connection has already been committed/aborted:
  blncommited = false;
  blnaborted = false;
}

// Destructor
pg_transaction::~pg_transaction() {
  // Check if commit or abort were called:
  if (!blncommited && !blnaborted) log_warning("A database transaction wasn't committed or aborted. Automatically aborted");
}

// Executing queries:
pg_result pg_transaction::exec(const string & strquery) {
  return connection.exec(strquery);
}

pg_result pg_transaction::exec(const string & strquery,
                               const pg_params & params) {
  return exec(format_string_with_vector(strquery, params, "?"));
}

// Committing the transaction:
void pg_transaction::commit() {
  if (connection.ptransaction == NULL) my_throw("Transaction is not setup!");
  connection.ptransaction->commit();
  blncommited = true;
}

// Aborting the transaction:
void pg_transaction::abort() {
  if (connection.ptransaction == NULL) my_throw("Transaction is not setup!");
  connection.ptransaction->abort();
  blnaborted = true;
}

/**************************************
    Type conversion functions:
**************************************/

// PGSQL query conversion

string datetime_to_psql(const datetime dtmdatetime) {
  string strdatetime = format_datetime(dtmdatetime, "%F %T");
  string strSQL = "to_timestamp('" + strdatetime + "', 'yyyy-mm-dd hh24:mi:ss')";
  return strSQL;
}

string date_to_psql(const datetime dtmdate) {
  string strDate = format_datetime(dtmdate, "%F");
  string strSQL = "to_timestamp('" + strDate + "', 'yyyy-mm-dd')";
  return strSQL;
}

string get_string_compare_psql(const string & str) {
  // A convention I follow - empty strings are stored in databases as NULL values.
  // So to compare an identifier or value with a field we need to take this into account
  // eg: SELECT * FROM tblTable WHERE strValue = 'Pit of Eternal Peril'
  // or: SELECT * FROM tblTable WHERE strValue IS NULL
  //     instead of (WHERE strValue = '')

  string strCompare = "";
  if (str=="")
    strCompare = "IS NULL";
  else {
    strCompare = "= " + string_to_psql(str);
  }
  return strCompare;
}

datetime parse_psql_datetime(const string & strpsql_datetime, const bool blnextract_date=false, const bool blnextract_time=false) {
  //
  // * Default value for dtmDefault is datetime_error;
  //
  // When a PSQL query returns a datetime field, it is in string format. Use this
  // function to translate a datetime field into a C++ datetime value. An exception
  // is thrown if there is an error parsing.
  // blnextract_date = True if this function must extract the date portion, leave
  //                   as false to extract only the time portion.
  // blnextract_time = True if this function must extract the time portion, leave
  //                   as false to only extract the date portion.

  // Setup the default return value
  datetime return_val = datetime_error;

  // Check if blnextract_date or blnextract_time is set:
  if (!blnextract_date && !blnextract_time) {
    // This function must either extract a date, a time, or both!
    LOGIC_ERROR;
  }

  // Setup some work variables
  int intYear, intMonth, intDay, intHour, intMinute, intSecond;
  intYear = intMonth = intDay = intHour = intMinute = intSecond = 0;

  // Check the string format
  const char * DateStr = strpsql_datetime.c_str();

  int intdatelen = strlen(DateStr);

  if (intdatelen < 10) {
    my_throw((string)"date/time string \"" + DateStr + "\" is too short!");
  }

  // We should have at least the date:
  // Year
  if (!(isdigit(DateStr[0]) &&
      isdigit(DateStr[1]) &&
      isdigit(DateStr[2]) &&
      isdigit(DateStr[3]) &&

  // Dividing "-" char
      DateStr[4]=='-' &&
  // Month
      isdigit(DateStr[5]) &&
      isdigit(DateStr[6]) &&
  // Dividing "-" char
      DateStr[7]=='-' &&
  // Day
      isdigit(DateStr[8]) &&
      isdigit(DateStr[9]))) {
    my_throw("Invalid postgresql date/time string \"" + strpsql_datetime + "\"");
  }

  // We have the date portion -> extract it
  intYear = atoi(strpsql_datetime.substr(0,4).c_str());
  intMonth = atoi(strpsql_datetime.substr(5,2).c_str());
  intDay = atoi(strpsql_datetime.substr(8,2).c_str());

  // == So we have the date - is there more string? ==
  if (intdatelen > 10) {
    // We have a valid number of additional characters... check them

    // Dividing " " char
    if (DateStr[10]!=' ') {
      my_throw("Char 10 must be a space!");
    }

    // Dividing space found. Are the next two characters "BC"?
    if ((DateStr[11]=='B') && (DateStr[12])=='C') {
      // " BC" was listed after the date. So there is no time component.
    } // end if ((DateStr[11]=='B') && (DateStr[12])=='C')
    else {
       // " BC" was not listed after the date. Look for a valid time string.
      if (!(isdigit(DateStr[11]) &&       // Hour
      isdigit(DateStr[12]) &&

      // Dividing ":" char
      DateStr[13]==':' &&

      // Minute
      isdigit(DateStr[14]) &&
      isdigit(DateStr[15]))) {
        my_throw("Invalid time portion!");
      }

      // Everything is ok up to the minute code. 2 Options now: a colon and a second, or a space and BC
      // - First get the date parts we have so far
      intHour = atoi(strpsql_datetime.substr(11,2).c_str());
      intMinute = atoi(strpsql_datetime.substr(14,2).c_str());

      intSecond = 0; // Default value unless the second actually is provided

      // Is the string  ":SS" format? (as opposed to " BC" format)
      if (DateStr[16]==':' && isdigit(DateStr[17]) && isdigit(DateStr[18])) {
        intSecond = atoi(strpsql_datetime.substr(17,2).c_str());
      } // end if
    } // END: if ((DateStr[11]=='B') && (DateStr[12])=='C')
  } // end if (intdatelen > 10)

  // Now build the datetime value and return it.
  // - Check the year portion if appropriate

  if (blnextract_date) {
    if (intYear < 1970) {
      my_throw("Year " + itostr(intYear) + " is invalid - I cannot process years before 1970");
    }
  }

  if (blnextract_date && blnextract_time) {
    return_val = make_datetime(intYear, intMonth, intDay, intHour, intMinute, intSecond);
  }
  else if (blnextract_date && !blnextract_time) {
    return_val = make_date(intYear, intMonth, intDay);
  }
  else if (!blnextract_date && blnextract_time) {
    return_val = make_time(intHour, intMinute, intSecond);
  }
  else LOGIC_ERROR; // Should never run!

  return return_val;
}

datetime parse_psql_datetime(const string & strdatetime) {
  return parse_psql_datetime(strdatetime, true, true);
}

datetime parse_psql_date(const string & strdate) {
  return parse_psql_datetime(strdate, true, false);
}

datetime parse_psql_time(const string & strtime) {
  return parse_psql_datetime(strtime, false, true);
}

string string_to_psql(const string & str) {
  // Take an input string, format it (add quotes, etc) so that it can be
  // appended into a PGSQL query
  string strSQL = "";
  if (str=="")
    strSQL = "NULL";
  else {
    strSQL = str;
    strSQL = replace(strSQL, "'", "''");  // Double the single quotes
    strSQL = replace(strSQL, "\\", "\\\\"); // Double the slash characters
    // Add single quots to the outside of the string

    strSQL = "'" + strSQL + "'";
  }
  return strSQL;
}

string time_to_psql(const datetime dtmtime) {
  string strSQL = "";
  string strTime = format_datetime(dtmtime, "%T");
  strSQL = "to_timestamp('" + strTime + "', 'hh24:mi:ss')";
  return strSQL;
}

#endif // END: #ifdef __linux__ // Postgresql access is only possible under linux at this time!

// Some postgresql-specific utility functions:
bool pg_table_exists(pg_conn_exec & conn, const string & table_name, const string & schema_name) {
  pg_result rs = conn.exec("SELECT tablename FROM pg_tables WHERE tablename = " + psql_str(lcase(table_name)) + " AND schemaname = " + psql_str(lcase(schema_name)));
  return rs.size() != 0;
}
