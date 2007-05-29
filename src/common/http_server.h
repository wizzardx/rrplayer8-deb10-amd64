/// @file
/// A basic wrapper for Window's internet API.

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <wininet.h>

// This class is an abstraction of an internet HTTP server.

#ifdef __WIN32__ // This is currently only available for Windows

// - Under Windows you will need to link against the "libwininet.a" library.

using namespace std;

class http_server {
public:
  // Constructor
  http_server(const string & strAddress); // eg: www.megatokyo.com

  // Destructor
  ~http_server();

  /// Synchronous GET - fetch an object on the HTTP server into a string.
  /// Warning - the pointer returned must be freed by the calling code!
  void get(const string & strResource, char * & pdata, long & lngsize);

private:
  string strServer;
  HINTERNET hSession;  ///< Internet session
  HINTERNET hService;  ///< Handle for talking to the webserver.
};

#endif

#endif
