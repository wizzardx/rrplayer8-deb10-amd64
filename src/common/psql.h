#ifdef __linux__ // postgresql is only usable under linux at this time!

#ifndef PSQL_H
#define PSQL_H

// Functions and utilities for easier postgres handling...

// Requirements:
// 1. lpqxx must be installed (compiled from sources)
// 2. Some of the #defines in /usr/include/pqxx/config.h must be commented out (they conflict with
//     kdevelop-generated #define lines)
// 3. Link your project against libpqxx "-lpqxx"
// 4. Exception handling must be enabled.
// 5. This include file path must be in your CXX flags: -I/usr/include/postgresql

#include <pqxx/connection.h>
#include <pqxx/transactionitf.h>
#include <pqxx/result.h>

#include <string>
#include "my_time.h"

using namespace std;
using namespace pqxx;

/***************************************************************************************
          A Connection wrapper
***************************************************************************************/

// Generate a connection string from parts:
string pg_create_conn_str(const string & strhost, const string & strport, const string strdbname, const string strusername, const string strpassword);

const int PG_RETRY_INFINITE = -1; // -1 means there is no limit to the number of retries

class pg_connection {
public:
  // Constructors
  pg_connection(); // Default constructor - no connection created, call open to do this

  // Destructor
  ~pg_connection();

  // Set the connect retry settings
  void set_connect_retries(const int intretries); // Default setting is PG_RETRY_INFINITE
  void set_connect_retry_interval(const int intseconds); // Default setting is 30 seconds.

  // Open a connection, keep retrying until successful or we've retried enough.
  void open(const string & strConn);
  // Close a connection.
  void close();
  // Check a connection - ie: if the connection is bad then attempt to reconnect.
  void check();
  // Is the connection open? This method does not actively check the connection or attempt to reconnect.
  bool ok();

  // And for executing queries through the connection:
  pg_result exec(const string & strquery);

  // Allow the client code to specify a calback function to be run when connection errors
  // are detected. Sometimes the database will be down for a long time...
  // The callback is called after each failed connection attempt.
  void call_on_connect_error(void(*func)());
private:
  string strconn; // Connection string to the database;
  Connection * pconn; // The wrapped libpqxx Connection object.
  TransactionItf * ptransaction; // The current transaction.

  // Client code's callback function to call when there are connect errors:
  typedef void (callback_func)();
  callback_func * client_conn_err_code;
  
  // Retry settings.
  int intmax_retries, intretry_interval;

  // Function used internally by open() and check():
  void establish_connection(); // Throws an exception if a database connection cannot be established.

  // Don't allow connections to be copied or assigned:
  pg_connection (const pg_connection & pg_connection);
  pg_connection operator = (const pg_connection & pg_connection);

  // Count the number of queries run. After every 100 queries executed, we close and
  // open the connection. This is because the memory used by a single connection will
  // use more and more memory in the postgresql backend process. This may be a leak
  // or some kind of background caching. But eventually an active database-using program
  // will use up a huge amount of postgresql backend memory!!!
  unsigned long long lngnum_exec_calls;

  // Call this function to switch between a NonTransaction and a Transaction:
  // These functions are mainly called by pg_transaction:
  friend class pg_transaction;
  void set_auto_commit_mode(const bool autocommit);
  bool blnauto_commit;
  void commit();  
};

/********************************************************************************************************
          A Result wrapper, to make porting from existing pg_recordset code easier


*********************************************************************************************************/

class pg_result {
public:
  // Copy constructor and assignment operator (copy from another pg_result object)
  pg_result(const pg_result & pg_res);
  pg_result& operator=(const pg_result & pg_res);

  // Destructor
  ~pg_result();

  // Field retrieval
  string field(const string & strfield_name, const char * strdefault_val = NULL) const; // Rethrows exceptions
  bool field_is_null(const string & strfield) const; // Rethrows exceptions

  // Resultset traversal:
  bool eof();
  void movenext(); // An exception is thrown if an attempt is made to move past the EOF.
  long recordcount();

  void movefirst() { introw_num = 1; };
  void movelast() { introw_num = recordcount(); };

  // Feedback for INSERT, UPDATE and DELETE statements:
  // - If command was INSERT of 1 row, return oid of inserted row:
  long inserted_oid() { return (*presult).inserted_oid(); }
  // - If command was INSERT, UPDATE, or DELETE, return number of affected rows
  //   Returns zero for all other commands
  long affected_rows() { return (*presult).affected_rows(); }

private:
  // Only pg_connection and pg_transaction objects can create a new instance from scratch
  // - ie, not by copying from another pg_result object:
  friend class pg_connection;

  // Constructor to create a pg_result object from a Result object:
  pg_result(const pqxx::Result res);

  int introw_num;
  Result * presult;
  string strsql; // The query which was executed to create this result.  
};

/***************************************************************************************
          A transaction wrapper
***************************************************************************************/

// This is basically a Connection wrapper. When you create it, it creates a new database
// connection, but setup in Transaction (not NonTransaction) mode.

class pg_transaction {
public:
  // Constructor:                                        
  pg_transaction(pg_connection & conn);

  // Executing queries:
  pg_result exec(const string & strquery);

  // Committing the transaction:
  void commit();

  // Aborting the transaction:
  void abort();
private:
  pg_connection connection;

  // Disable some calls:
  pg_transaction (const pg_transaction & pg_transaction);            // A(B)
  pg_transaction operator = (const pg_transaction & pg_transaction); // =
};

/**************************************
    Type conversion functions:
**************************************/

// PGSQL query conversion
string datetime_to_psql(const datetime dtmdatetime);
string date_to_psql(const datetime dtmdate);
string get_string_compare_psql(const string & str);

datetime parse_psql_datetime(const string & strdatetime);
datetime parse_psql_date(const string & strdate);
datetime parse_psql_time(const string & strtime);

string string_to_psql(const string & str);
string time_to_psql(const datetime dtmtime);

// Some macros for common PSQL query conversions:


// - Date & Time:
#define psql_now datetime_to_psql(now())
#define psql_date date_to_psql(now())
#define psql_time time_to_psql(now())

// Strings:
#define psql_str(str) string_to_psql(str)

#endif // END: #ifndef RR_PSQL_H
#endif // END: #ifdef __linux__ // postgresql is only usable under linux at this time!



