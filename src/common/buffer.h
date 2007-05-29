/// @file
/// Basic buffer handling

#ifndef BUFFER_H
#define BUFFER_H

#include <string>

using namespace std;

// Memory buffer <--> File
/// Save a buffer to a file
void save_buffer(char * data, long lngsize, const string & strfile);

/// Read a file into a buffer: Caution: Buffer must be freed by the caller.
void load_buffer(const string & strfilename, char * & pdata, long & lngsize);

// Buffer searching. Functions return -1 if the string can't be found.
long buffer_search_backwards(char * data, long lngsize, const string & strsearch); ///< Search a buffer backwards for a string. Returns -1 if the string can't be found.
long buffer_search_forwards(char * data, long lngsize, const string & strsearch);  ///< Search a buffer forwards for a string. Returns -1 if the string can't be found.

#endif
