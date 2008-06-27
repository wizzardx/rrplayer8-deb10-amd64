
#include "buffer.h"
#include "exception.h"
#include <sys/stat.h> // fstat

// Basic file I/O - save a buffer to a file or read a file into a buffer.

// Save a buffer to a file.
void save_buffer(char * Data, long lngSize, const string & strFileName) {
  FILE *stream;

  // Open the file.
  stream = fopen (strFileName.c_str(), "wb");
  if (stream == NULL) my_throw("Could not open file for writing: " + strFileName);

   // File is open, so write
  long lngWritten = fwrite(Data, 1, lngSize, stream);

  if (lngWritten != lngSize) my_throw("There was an error writing this file! -> " +strFileName);

  fclose(stream);
}

// Read a file into a buffer: Caution: Buffer must be freed by the caller.
void load_buffer(const string & strFileName, char * & pData, long & lngSize) {
  pData = NULL;

  FILE * stream = fopen(strFileName.c_str(), "rb");
  if (stream == NULL) my_throw("Could not open file! -> " + strFileName);

  // File was opened.

  // Fetch the file size
  struct stat file_stats;
  if (fstat(fileno (stream), &file_stats) != 0) my_throw("File stats not retrieved!");

  // File stats retrieved successfully. Create a buffer of this size and
  // populate it with data from the file.
  lngSize = file_stats.st_size;
  pData = (char*)malloc(lngSize+1); // allocate total size + 1 (for a \0)

  // Read data into the buffer
  long lngRead = fread(pData, 1, lngSize, stream);

  if (lngRead != lngSize) LOGIC_ERROR;

  // Place a \0 at the end, so that if this is text data, the string
  // will end here (just in case)
  pData[lngSize]=0;

  // Now close the file
  fclose(stream);
}

// Buffer searching. Functions return -1 if the string can't be found.
long buffer_search_backwards(char * data, long lngsize, const string & strsearch) {
  if (data == NULL) LOGIC_ERROR;
  if (lngsize < 0) LOGIC_ERROR;
  if (strsearch == "") LOGIC_ERROR;

  const char * chsearch      = strsearch.c_str();
  const char intsearch_len   = strsearch.length();

  // Start the backwards search at the end of the buffer.
  long lngsearch_start = lngsize - intsearch_len;

  // Search backwards through the buffer...
  long lngfound_pos = -1; // Position in the buffer where the search string was found.
  for (long lngpos=lngsearch_start; lngpos >=0 && lngfound_pos==-1; --lngpos) {
    long lngcompare_pos;
    for (lngcompare_pos=0;
         (lngcompare_pos < intsearch_len) &&
         (data[lngpos + lngcompare_pos] == chsearch[lngcompare_pos]);
         ++lngcompare_pos); // The for loop does all the work...
    if (lngcompare_pos >= intsearch_len) {
      lngfound_pos = lngpos;
    } // end if
  } // end for

  // Return the position the string was found at (or -1 if not found)
  return lngfound_pos;
}

long buffer_search_forwards(char * data, long lngsize, const string & strsearch) {
  if (data == NULL) LOGIC_ERROR;
  if (lngsize <= 0) LOGIC_ERROR;
  if (strsearch == "") LOGIC_ERROR;

  const char * chsearch      = strsearch.c_str();
  const char intsearch_len   = strsearch.length();

  // Search the buffer:
  long lngfound_pos  = -1; // Position where string was found.
  long lngcompare_pos=0; // Current "comparision" position.
  for (long lngpos=0; lngpos < lngsize  && lngfound_pos==-1; ++lngpos) {
    if (data[lngpos] == chsearch[lngcompare_pos]) {
      ++lngcompare_pos;
    }
    else {
      lngcompare_pos = 0;
    }
    if (lngcompare_pos >= intsearch_len) {
      lngfound_pos = lngpos-intsearch_len+1;
    }
  }

  return lngfound_pos;
}
