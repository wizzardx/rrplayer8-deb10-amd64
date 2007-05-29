// Implementation for http_server
// - Uses wininet

#include "http_server.h"
#include "exception.h"

// This class is an abstraction of an internet HTTP server.

#ifdef __WIN32__ // This is currently only available for Windows

// Constructor
http_server::http_server(const string & strAddress) { // eg: www.megatokyo.com
  // Step 1: Clear the data members
  hSession = 0; // Internet session
  hService = 0; // Handle for talking to the webserver.

  strServer = strAddress;

  // Step 2: Get an internet session
  hSession = InternetOpen("MyApp", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
  if (!hSession) my_throw("Unable to get an internet session!");

  // Step 3: Connect to the server
  hService = InternetConnect(hSession, strServer.c_str(),
                             INTERNET_DEFAULT_HTTP_PORT, NULL, NULL,
                             INTERNET_SERVICE_HTTP, 0, 0);
  if (!hService) my_throw("Unable to connect to server!");
}

// Destructor
http_server::~http_server() {
  // Step 1: Free the internet session (also disconnects from any open services?)
  InternetCloseHandle(hSession);

  // Step 2: Reset the data members
  hSession = 0; // Internet session
  hService = 0; // Handle for talking to the webserver.
}

void http_server::get(const string & strResource, char * & pData, long & lngSize) {
  lngSize = 0;             // 0 bytes downloaded so far...

  if (!hService) LOGIC_ERROR;

  HINTERNET hHttpRequest;
// - This commented-out version does not use the cache.
//  hHttpRequest = HttpOpenRequest(hService, "GET", strResource.c_str(), NULL,
//                                 NULL, NULL, INTERNET_FLAG_RELOAD, 0);

  string strReferer = (string)"http://" + strServer;

  hHttpRequest = HttpOpenRequest(hService, "GET", strResource.c_str(), NULL,
                                 strReferer.c_str(), NULL, INTERNET_FLAG_RELOAD, 0);
  if (!hHttpRequest) LOGIC_ERROR;

  if (!HttpSendRequest(hHttpRequest, NULL, 0, NULL, 0)) my_throw("An error sending http request");

  // Now loop and retrieve the incoming data
  DWORD dwBytesAvailable, dwBytesRead;
  InternetQueryDataAvailable(hHttpRequest, &dwBytesAvailable, 0, 0);
  long lngAppendPos = 0; // Where the next block of data will be written in the buffer

  while (dwBytesAvailable > 0) {
    // Grow the buffer
    lngSize += dwBytesAvailable;
    pData = (char*)realloc(pData,lngSize+1); // allocate total size + 1 (for a \0)

    // Read data into the buffer
    InternetReadFile(hHttpRequest, &pData[lngAppendPos], dwBytesAvailable, &dwBytesRead);

    // Place a 0 at the end, so that if this is HTML data, the string
    // will end here (just in case)
    pData[lngSize]=0;

    // Update the position for the next block to be appended from:
    lngAppendPos = lngSize;

    // See if there are more bytes to read...
    InternetQueryDataAvailable(hHttpRequest, &dwBytesAvailable, 0, 0);
  }

  if (lngSize <= 0) my_throw("Did not fetch any data!");
}

#endif

