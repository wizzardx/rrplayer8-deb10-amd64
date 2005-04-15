/***************************************************************************
                          mp3_handling.h  -  description
                             -------------------
    begin                : Tue Jul 8 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifdef __linux__

#ifndef MP3_HANDLING_H
#define MP3_HANDLING_H

#include <string>
#include <stdio.h>

/**
  *@author David Purdy
  */

using namespace std;

void append_mp3(string strDestFile, string strSourceFile);

// Shell the "checkmp3" utility, throw an exception if the MP3 check fails!
void check_mp3(const string & strMP3);

void extract_mp3s(const string strSourceFile, const string strDestFileStart, bool * pblnSpaceAtStart, bool * pblnSpaceAtEnd, bool blnOnlyTrim=false);

// Added in version 1.12 - ExtractMP3s is called with blnOnlyTrim=True when the Extraction logic is to ONLY strip
// any (possibly minute) leading and trailing spaces from the MP3. The source file will be overwritten with the
// trimmed version.
// - *Note* - call MP3_Trim() instead, the function name makes more sense, and the "blnTrimOnly" argument for
// ExtractMP3s is a temporary hack until the logic is tidied up.

void mp3_strip_tags(const string strMP3Path);
void mp3_trim(const string strMP3Path, const string strTempStart); // Extremely silence-sensitive removal of spaces from the start and end
                                                                   // - strTempStart is the beginning part of the
void normalize_mp3(const string & strMP3);
                                                                                                             
#endif
#endif
