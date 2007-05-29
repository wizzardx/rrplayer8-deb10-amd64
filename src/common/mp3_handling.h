/// @file
/// mp3-related functions
/// @see media_splitter.h

#ifdef __linux__

#ifndef MP3_HANDLING_H
#define MP3_HANDLING_H

#include <string>
#include <stdio.h>

/**
  *@author David Purdy
  */

using namespace std;

/// Append one mp3 to the end of another
void append_mp3(string strDestFile, string strSourceFile);

/// Shell the "checkmp3" utility, throw an exception if the MP3 check fails!
void check_mp3(const string & strMP3);

/// Scan an MP3 for absolute silences, and split the MP3 at these points.
void extract_mp3s(const string & strSourceFile, const string & strDestFileStart, bool * pblnSpaceAtStart, bool * pblnSpaceAtEnd, bool blnOnlyTrim=false);

// Added in version 1.12 - ExtractMP3s is called with blnOnlyTrim=True when the Extraction logic is to ONLY strip
// any (possibly minute) leading and trailing spaces from the MP3. The source file will be overwritten with the
// trimmed version.
// - *Note* - call MP3_Trim() instead, the function name makes more sense, and the "blnTrimOnly" argument for
// ExtractMP3s is a temporary hack until the logic is tidied up.

void mp3_strip_tags(const string & strMP3Path);
void mp3_trim(const string & strMP3Path, const string & strTempStart); // Extremely silence-sensitive removal of spaces from the start and end
                                                                       // - strTempStart is the beginning part of the

// Return the length in seconds
int get_mp3_length(const string & strfile);

// Return the bitrate in kbps
int get_mp3_bitrate(const string & strfile);

// Run mp3gain on an MP3:
void mp3gain(const string & strMP3);

// Convert an MP3 to WAV and back to MP3.
// - Useful for eg, compiling MP3s from other MP3s.
void reencode_mp3(const string & strMP3);

// Extract lyrics tag from an MP3
string get_mp3_lyrics_tag(const string & strmp3);

#endif
#endif
