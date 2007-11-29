/// @file
/// Functions and utilities for easier postgres handling.
/// To use this library:
/// - Install libpqxx-dev
/// - Include the following CPPFLAGS: -I /usr/include/postgresql/
/// - Link against the following libraries: /usr/lib/libpqxx.so
/// - Some of the #defines in /usr/include/pqxx/config.h must be commented out (they conflict with
///   kdevelop-generated #define lines)
/// - Exception handling must be enabled.

#ifdef __linux__ // postgresql is only usable under linux at this time!

#ifndef PSQL_H
#define PSQL_H

#include <pqxx/connection>
#include <pqxx/transaction_base>
#include <pqxx/result>

//#include <pqxx/connection.hxx>
//#include <pqxx/transaction_base.hxx>
//#include <pqxx/result.hxx>

#include <string>
#include "my_time.h"

using namespace std;
//using namespace pqxx;

/***************************************************************************************
          A Connection wrapper
***************************************************************************************/

class pg_result; // Forward declaration

// Abstract base class for pg_connection and pg_transaction. Used to allow passing objects of either
// type to functions that only need to call the "exec" method:
class pg_conn_exec {
public:
  virtual pg_result exec(const string & strquery)=0;
  virtual ~pg_conn_exec() {};
};

/// Generate a connection string from parts:
string pg_create_conn_str(const string & strhost, const string & strport, const string strdbname, const string strusername, const string strpassword);

const int PG_RETRY_INFINITE = -1; ///< -1 means there is no limit to the number of retries

class pg_connection : public pg_conn_exec {
public:
  // Constructors
  pg_connection(); ///< Default constructor - no connection created, call open to do this

  // Destructor
  virtual ~pg_connection();

  // Set the connect retry settings
  void set_connect_retries(const int intretries); ///< Default setting is 0 (no retries)
  void set_connect_retry_interval(const int intseconds); ///< Default setting is 30 seconds.

  /// Open a connection, keep retrying until successful or we've retried enough.
  void open(const string & strConn = "");
  /// Close a connection.
  void close();
  /// Check a connection - ie: if the connection is bad then attempt to reconnect.
  void check();
  /// Is the connection currently open? This method does not actively check the connection or attempt to reconnect.
  bool isopen();

  /// And for executing queries through the connection:
  virtual pg_result exec(const string & strquery);

  /// Allow the client code to specify a calback function to be run when connection errors
  /// are detected. Sometimes the database will be down for a long time...
  /// The callback is called after each failed connection attempt.
  void call_on_connect_error(void(*func)());
private:
  string strconn; ///< Connection string to the database;
  pqxx::connection * pconn; ///< The wrapped libpqxx Connection object.
  pqxx::transaction_base * ptransaction; ///< The current transaction.

  /// Client code's callback function to call when there are connect errors:
  typedef void (callback_func)();
  callback_func * client_conn_err_code;

  /// Retry settings.
  int intmax_retries, intretry_interval;

  /// Function used internally by open() and check().
  /// Throws an exception if a database connection cannot be established.
  void establish_connection();

  // Don't allow connections to be copied or assigned:
  pg_connection (const pg_connection & pg_connection);
  pg_connection operator = (const pg_connection & pg_connection);

  /// Count the number of queries run. After every 100 queries executed, we close and
  /// open the connection. This is because the memory used by a single connection will
  /// use more and more memory in the postgresql backend process. This may be a leak
  /// or some kind of background caching. But eventually an active database-using program
  /// will use up a huge amount of postgresql backend memory!!!
  unsigned long long lngnum_exec_calls;

  // Call this function to switch between a NonTransaction and a Transaction:
  // These functions are mainly called by pg_transaction:
  friend class pg_transaction;
  void set_auto_commit_mode(const bool autocommit);
  bool blnauto_commit;
  void commit();
};

/// A Result wrapper, to make porting from existing pg_recordset code easier
class pg_result {
public:
  // Copy constructor and assignment operator (copy from another pg_result object)
  pg_result(const pg_result & pg_res);
  pg_result& operator=(const pg_result & pg_res);

  /// Destructor
  ~pg_result();

  // Field retrieval
  string field(const string & strfield_name, const char * strdefault_val = NULL) const;
  bool field_is_null(const string & strfield) const;

  // Resultset traversal:
  /// Returns true while there is still information left in the recordset
  inline operator bool() const  { return (unsigned)introw_num <= (*presult).size(); }

  void operator ++(int); /// Move to the next record
  inline long size() const  { return presult->size(); };
  inline bool empty() const { return presult->empty(); };

  inline void movefirst() { introw_num = 1; };
  inline void movelast() { introw_num = presult->size(); };

  /// Feedback for INSERT, UPDATE and DELETE statements.
  /// If command was INSERT of 1 row, return oid of inserted row
  inline long inserted_oid() const { return (*presult).inserted_oid(); }

  /// If command was INSERT, UPDATE, or DELETE, return number of affected rows.
  /// Returns zero for all other commands
  inline long affected_rows() const { return (*presult).affected_rows(); }

private:
  /// Only pg_connection and pg_transaction objects can create a new instance from scratch.
  /// ie, not by copying from another pg_result object:
  friend class pg_connection;

  /// Constructor to create a pg_result object from a Result object:
  pg_result(const pqxx::result res);

  int introw_num;
  pqxx::result * presult;
  string strsql; ///< The query which was executed to create this result.
};

/***************************************************************************************
          A transaction wrapper
***************************************************************************************/

/// This is basically a Connection wrapper. When you create it, it creates a new database
/// connection, but setup in Transaction (not NonTransaction) mode.
class pg_transaction : public pg_conn_exec {
public:
  /// Constructor:
  pg_transaction(pg_connection & conn);

  /// Destructor
  virtual ~pg_transaction();

  /// Execute a query
  virtual pg_result exec(const string & strquery);

  /// Commit the transaction:
  void commit();

  /// Abort the transaction:
  void abort();
private:
  pg_connection connection;

  // Disable some calls:
  pg_transaction (const pg_transaction & pg_transaction);            // A(B)
  pg_transaction operator = (const pg_transaction & pg_transaction); // =

  // And variables to store whether the connection has already been committed/aborted:
  bool blncommited;
  bool blnaborted;
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

// Foreign key refs (-1 changes to "NULL")
#define psql_fkey(fkey) ((fkey == -1) ? "NULL" : ltostr(fkey))

// Boolean fields:
#define psql_bool(bln) (bln ? "'t'" : "'f'")

// Some postgresql-specific utility functions:
bool pg_table_exists(pg_conn_exec & conn, const string & table_name, const string & schema_name = "public");

#endif // END: #ifndef RR_PSQL_H
#endif // END: #ifdef __linux__ // postgresql is only usable under linux at this time!

