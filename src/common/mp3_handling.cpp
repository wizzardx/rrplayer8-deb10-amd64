/***************************************************************************
                          mp3_handling.cpp  -  description
                             -------------------
    begin                : Tue Jul 8 2003
    copyright            : (C) 2003 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifdef __linux__ // mp3 handling is only possible under linux at this time...

#include "mp3_handling.h"
#include <unistd.h>
#include "string_splitter.h"
#include "exception.h"
#include "my_string.h"
#include "system.h"
#include "file.h"
#include "temp_dir.h"
#include "buffer.h"
#include <sys/stat.h> // fstat

// Some terminology and explanations: When software such as cooledit blanks out areas of
// an MP3 file with absolute silence, this creates patterns inside the MP3 file which can be
// detected and be used to extract the sections of sound which are not part of the blanked out
// "absolute silence" area.
//
// The constants below are pretty self-explanatory.

// Some constants for MP3 extraction:

const int intSilenceChar1                        = 255;  // The ASCII code for the "dead silence" character
                                                         // inserted by many mp3 encoders. We use this
                                                         // to determine if we are in a "long silence run".
                                                         // A long silence run is an uninterrupted sequence
                                                         // of this silence character.
const int intSilenceChar2                        = 0;    // Another character which is sometimes used for the
                                                         // same purpose as the above. Implication:
                                                         // long streams of FF and 00 count as long silence runs.
                                                         // (v1.07)

// A handy macro for logic to tell if a byte is one of the above? (if the silence codes change) then this
// centralises changes to this point in the code
# define IS_SILENCE_CHAR(c) (((c) == intSilenceChar1 || (c) == intSilenceChar2) ? true : false)

const int intLongSilenceRunThreshold = 100; // This many silence characters in a run of
                                            // silence characters make up a long silence
                                            // run.
const int intLongNoiseRunThreshold = 5*1024;  // This many characters must elapse since the
                                              // most recent character within a long silence
                                              // run before the current position is considered
                                              // to be within a long noise run.
                                              // Changed in version 1.06 from
                                              // 200 bytes to 5 kb - this is so that tiny chunks
                                              // of non-silence are ignored.

const long lngSilenceAreaThreshold = 40000;  // This many characters must have elapsed since
                                             // the most recent last long noise run position
                                             // (and we must also be in a long silence run)
                                             // before we can safely treat the current
                                             // position as a silence area.

                                             // (v1.07) Upped the threshhold from 20k to 40k
                                             //  (k not kb, ie 1000 not 1024)
                                             //  - now the smaller silence areas will be ignored.
                                             //    I guestimate that 40k is around 3 seconds of
                                             //    absolute silence with the cooledit MP3 settings that
                                             //    Radio Retail currently uses.

const int lngFrameLength = 418; // (v1.12) - We are assuming this is the byte length of 1 MP3 frame.
                                //    - This figure is used to help remove truncation of sounds when extracting...

void do_qmp3cut(long lngextractstart, long lngextractlen, const string strdestfile, const string strsourcefile) {
  // Use qmp3cut to extract mp3s from another mp3.

  // The command-line tool qmp3cut will be confused by files with spaces in their path. Check for this
  if ((strdestfile.find(" ", 0) != strdestfile.npos) ||
      (strsourcefile.find(" ", 0) != strsourcefile.npos)) {
    my_throw("Cannot process MP3s with spaces in their name or path");
  }

  // To avoid truncating frames, we alter the extract start and length so that if they are truncated to
  // another frame boundary, they will still include the entire sound...
  lngextractstart -= lngFrameLength;
  lngextractlen += lngFrameLength * 2;

  // Clip extractstart to 0 if it is now negative...
  if (lngextractstart < 0) {
    lngextractstart = 0;
  }

  // Build the command string to run.
  string strCommand="qmp3cut ";
  if (lngextractstart > 2048) {
    // Only include the start position to extract from if it is not at the very beginning of the
    // file. This is because if we tell qmp3cut to start at byte position 1, it complains about
    // frame 0 out of range. If we leave out the -B parameter it will extract from the start
    // of the file anyway.
    strCommand += " -B " + itostr(lngextractstart) + "b ";
  }

  // And the rest of the command.
  strCommand += " -s " + itostr(lngextractlen) + "b -o " + strdestfile + " " + strsourcefile + " &> /dev/null";
  string strCmdOut = "";
  if (system_capture_out(strCommand, strCmdOut) != 0) {
    my_throw("Could not extract mp3 sub-section. " + strCmdOut + ". The command that failed: " + strCommand);
  }
}

void ExtractMP3sFromBuffer(const unsigned char * buffer, const long lngSize, const string strDestFileStart, const string TEST_SOURCE_FILE, bool * pblnLongSilenceAreaAtStart, bool * pblnLongSilenceAreaAtEnd, bool blnOnlyTrim) {
  // Scan a buffer read from an MP3 file, search for sub-mp3s, and extract them
  // - Added in version 1.06 - this function now also returns the blnLongSilenceAreaAtStart
  //                                         and blnLongSilenceAreaAtEnd parameters. If either of these
  //                                         are set to true by this function, it means that an additional
  //                                         price/name/product/whatever needs to be placed before/after
  //                                         the extraced sections in the final MP3.
  //   - this parameter is set to true if a "long silence area" is found at the end of the MP3. If

  // - Added in version 1.12 - argument "blnOnlyTrim" - if this is set, then instead of
  //                                         extracting sub-sections, the logic will do an extremely silence-sensitive
  //                                         removal of leading and trailing spaces.

  // Initialize variables
  int bytInput = -1;       // Character at this position within the MP3
  int intPrevByte = -1;    // The character that was read before this one.
  long lngCharsInRun = -1; // Count how many silence characters (or non-silence characters)
                           // there are in the current "run"
  bool blnInLongSilenceRun = false;         //Shows if the current byte is within a long silence run
  long lngMostRecentLongSilenceRunPos = -1; //The position of the most recent char in a long silence run

  long lngLongSilenceRunStart = -1; // The starting position of the most recent
                                    // long silence run

  long lngMostRecentLongNoiseRunPos = -1; // The position of the most recent char within a long noise run
  bool blnInLongNoiseRun = false;         // We've decided that the last silence run char was long enough ago...
  long lngLongNoiseRunStart = -1;         // The position of the most recent char in a long
                                          // noise run

  long lngNoiseWriteStartMarker = -1;  // The position of the first byte of a noise
                                       // block that is to be written to an MP3

  bool blnInSilenceArea = false;       // Shows if we're in a silence area, meaning
                                       // not within a long noise run ( a safe distance
                                       // after the most recent one. Any dead silences
                                       // between closely consecutive long noise runs
                                       // are not counted as silence areas.
  long lngMostRecentSilenceAreaPos = -1; // The most recent position that is within a
                                         // silence area
  long lngSilenceAreaStart = -1; // The most recent position that is with a
                                 // silence area
  long lngSilenceAreaCount = 0;  // The number of silence areas that have
                                 // occured - tallied up as we process the buffer.

  long lngExtractedCount = 0; // Counts how many MP3s have been extracted (v1.07)
  long lngPos = -1;           // Current position in the MP3 buffer. 0 -> size-1.

  // Added in version 1.12 - If we are in "Only Trim" mode, then the silence area detection becomes
  // a lot more sensitive. We in fact use any occurances of silence character runs...
  long lngSilenceAreaThreshold_This = lngSilenceAreaThreshold; // The value to be used in this
                                                               // call of the function...
  if (blnOnlyTrim) {
    // We are in "Trim" mode - change to "super-sensitive" mode...
    lngSilenceAreaThreshold_This = intLongSilenceRunThreshold; // Assume that we're in a "long silence" area as
                                                               // soon as a silence character run is found.
  }

  // Clear the flags which are set if a long silence area is found at the start or the end of
  // an MP3...
  *pblnLongSilenceAreaAtStart = *pblnLongSilenceAreaAtEnd = false;

  // While were not at the end of the MP3 file
  for (lngPos=0;lngPos<lngSize;lngPos++) {
    // Get a character
    bytInput = unsigned (buffer[lngPos]);
    if (IS_SILENCE_CHAR(bytInput)) {
      // We're in a run of silence charcters
      if (IS_SILENCE_CHAR(intPrevByte)) {
        // The silence run started earlier, count this char as well
        ++lngCharsInRun;
      }
      else {
        // This is the first silence character in the run
        lngCharsInRun = 1;
      }
    }
    else {
      // We're in a run of noise (ie, non-silence) characters
      if (intPrevByte != -1 && IS_SILENCE_CHAR(intPrevByte)) {
        // The noise run started earlier, count this char as well
        ++lngCharsInRun;
      }
      else {
        // This is the first noise character in the run
        lngCharsInRun = 1;
      }
    }

    // Are we in a long silence run?
    if ((IS_SILENCE_CHAR(bytInput)) &&
       (lngCharsInRun >= intLongSilenceRunThreshold)) {
      // Remember the most recent position within the long silence run
      lngMostRecentLongSilenceRunPos = lngPos;
      if (!blnInLongSilenceRun) {
        // We just started the long silence run. Remember the start position
        lngLongSilenceRunStart = lngPos - intLongSilenceRunThreshold + 1;
      }
      blnInLongSilenceRun = true;
    }
    else {
      blnInLongSilenceRun = false;
    }

    // Or are we in a long noise run? (Short sequences of non-silence chars do not count as a
    // long noise run. Likewise, short sequences of silence chars do not interrupt a long noise run.
    if ((!blnInLongSilenceRun) &&
//       (lngMostRecentLongSilenceRunPos != -1) &&
       (lngPos - lngMostRecentLongSilenceRunPos + 1 >= intLongNoiseRunThreshold)) {

      // If a "long noise run" has just started, then keep track of where the noise area starts
      lngMostRecentLongNoiseRunPos = lngPos;
      if (!blnInLongNoiseRun) {
        if (lngMostRecentLongSilenceRunPos==-1) {
          // There is no recent long silence run, most likely the long noise run started
          // from the beginning of the file.
          lngLongNoiseRunStart=0;
        }
        else {
          // There was an earlier long silence run, mark the last position of the silence
          // to be used as the start of our next MP3 extract.
          lngLongNoiseRunStart = lngMostRecentLongSilenceRunPos;
        }
        // Mark the start of the area to be written. If the write marker
        // is already set then this newly detected noise area will be included in the
        // area of the file that will be written to another MP3
        if (lngNoiseWriteStartMarker == -1) {
          lngNoiseWriteStartMarker = lngLongNoiseRunStart + 1;
        }
      }
      blnInLongNoiseRun = true;
      blnInSilenceArea = false;
    }
    else {
      blnInLongNoiseRun = false;
    }

    // Are we in a silence area?
    if (blnInLongSilenceRun &&
       (lngPos - lngMostRecentLongNoiseRunPos + 1 >= lngSilenceAreaThreshold_This)) {
      // - We're safely out of any noise area, including any silence runs that
      // - may have been found within the noise (for example a space between
      // - words)
      lngMostRecentSilenceAreaPos = lngPos;
      if (!blnInSilenceArea) {
        lngSilenceAreaStart = lngMostRecentLongNoiseRunPos;
        if (lngSilenceAreaStart < 0) {
          lngSilenceAreaStart = 0;
        }

        // Also increment the Long Silence Area counter (v1.07)
        ++lngSilenceAreaCount;

        // Change in version 1.12 - Added the "blnTrimOnly" option. If blnTrimOnly is set, then
        // we do not extract MP3 sub-sections in the loop. The loop's purpose is now to prepare
        // the final "remaining noise flush" step so that it outputs the entire MP3, minus any
        // (minute) leading and tailing spaces.

        // The start of a silence area was found. If there is a noise block
        // waiting to be written then do that and clear the write start marker
        if ((lngNoiseWriteStartMarker != -1) && (!blnOnlyTrim)) {
          long lngExtractFrom=lngNoiseWriteStartMarker;
          long lngExtractLength= lngSilenceAreaStart - lngNoiseWriteStartMarker + 1;

          ++lngExtractedCount;
          do_qmp3cut(lngExtractFrom, lngExtractLength, strDestFileStart + itostr(lngExtractedCount) + ".mp3", TEST_SOURCE_FILE);

          // Clear the marker
          lngNoiseWriteStartMarker = -1;

          // Now that we've extracted an MP3, check if it was the first MP3 to be extracted,
          // and if there was a "long silence space" earlier. (v1.07)
          if ((lngExtractedCount==1) && (lngSilenceAreaCount > 1)) {
            // This is the first MP3, and there was a long silence before the start
            // - Note that this code always runs during a long-silence run because
            //   that is how we know we've left the last noise area. So if there was a
            //   a space at the start, and we just extracted the first MP3 segment, then the
            //   Silence Area Count will be 2 (not 1)

            // - Mark the appropriate flag for the calling function to see.
            *pblnLongSilenceAreaAtStart = true;
          }
        }
      }
      blnInSilenceArea = true;
    }
    intPrevByte = bytInput;
  }

  // Now that we're at the end of the MP3, flush any remaining MP3 data to a sub-file
  // Change in version 1.06 - also check that the last section of noise is long enough
  // to be extracted into an MP3. If there is a "long noise run" at the end of the file, it
  // still may be to short to make up a good MP3 file to be used for compilation

  if (lngNoiseWriteStartMarker != -1) {
    // Calculate the start extract position and length, and round down to the nearest 16.
//    long lngExtractFrom = (lngNoiseWriteStartMarker/16)*16;
//    long lngExtractLength = ((lngPos - lngNoiseWriteStartMarker + 1)/16)*16;
    long lngExtractFrom=lngNoiseWriteStartMarker;

    // Change in 1.12 - The last noise to be extracted, may still have a silence after it.
    // long lngExtractLength=lngPos - lngNoiseWriteStartMarker + 1;
    long lngExtractLength=lngMostRecentLongNoiseRunPos - lngNoiseWriteStartMarker + 1;

    ++lngExtractedCount;
    do_qmp3cut(lngExtractFrom, lngExtractLength, strDestFileStart + itostr(lngExtractedCount) + ".mp3", TEST_SOURCE_FILE);

    // We cut the MP3. Now some special handling if we are in "blnOnlyTrim" mode.
    if (blnOnlyTrim) {
      // Check 1: lngExtractedCount must be 1
      if (lngExtractedCount != 1) {
        my_throw("I was trimming an MP3, but somehow extracted " + itostr(lngExtractedCount) + " MP3s instead of the expected 1 MP3.");
      }

      // Check 2: The extracted file must exist
      if (!file_exists(strDestFileStart + itostr(lngExtractedCount) + ".mp3")) {
        my_throw("I could not find an MP3 that I supposedly just extracted: " + strDestFileStart + itostr(lngExtractedCount) + ".mp3");
      }

      // Finally: Replace the source MP3 with the extracted section, which in fact is the original file, minus
      // any (possibly extremely tiny) leading and tailing spaces.
      cp(strDestFileStart + itostr(lngExtractedCount) + ".mp3", TEST_SOURCE_FILE);

      // Erase the leftover temp file...
      remove((strDestFileStart + itostr(lngExtractedCount) + ".mp3").c_str());
    }

    // Clear the marker
    lngNoiseWriteStartMarker = -1;
  }
  else {
    // We didn't extract an MP3 consisting of the last part of the file.
    // - Were we in a long silence area towards the end of the file? (v1.07)
    // If we were in a silence area at the end, and at least 1 MP3 has been extracted, then
    // the source MP3 had a space at the end.
    *pblnLongSilenceAreaAtEnd = lngExtractedCount > 0 && !blnInLongNoiseRun;
  }
}

void extract_mp3s(const string & strSourceFile, const string & strDestFileStart, bool * pblnSpaceAtStart, bool * pblnSpaceAtEnd, bool blnOnlyTrim) {
  // Check that the tools we need for mp3 extraction and concatenation are installed
  // (v1.07) - added the blnSpaceAtStart and blnSpaceAtEnd parameters. These parameters
  // reflect whether the "filler" MP3 (price/name/product/whatever) must be prepended or appended
  // to the final MP3.

  // - Added in version 1.12 - argument "blnOnlyTrim" - if this is set, then instead of
  //                                         extracting sub-sections, the logic will do an extremely silence-sensitive
  //                                         removal of leading and trailing spaces.

  if (!file_exists("/usr/bin/qmp3cut")) {
    my_throw("A required tool qmp3cut was not found! Please install the debian package \"quelcom\".");
  }

  if (!file_exists("/usr/bin/qmp3join")) {
    my_throw("A required tool qmp3join was not found! Please install the debian package \"quelcom\".");
  }

  // Open the source file.
  FILE * pFile;
  long lngSize;
  unsigned char * buffer;
  pFile = fopen((strSourceFile).c_str(), "rb");
  if (pFile==NULL) {
    my_throw("Could not open MP3 file " + strSourceFile + " for reading.");
  }

  // Obtain the file size.
  fseek (pFile , 0 , SEEK_END);
  lngSize = ftell (pFile);
  rewind (pFile);

  // allocate memory to contain the whole file.
  buffer = (unsigned char*) malloc(lngSize);
  if (buffer == NULL) {
    fclose(pFile);
    my_throw("Could not allocate a memory buffer for " + strSourceFile);
  }

  // copy the file into the buffer.
  fread (buffer,1,lngSize,pFile);

  /*** the whole file is loaded in the buffer. ***/

  // Close the file.
  fclose (pFile);

  // Now start processing the buffer.
  ExtractMP3sFromBuffer(buffer, lngSize, strDestFileStart, strSourceFile, pblnSpaceAtStart, pblnSpaceAtEnd, blnOnlyTrim);
  free (buffer); // Free the buffer.
}

void MP3_Trim(const string strMP3Path, const string strTempStart) {
  // Extremely silence-sensitive removal of spaces from the start and end
  // Call ExtractMP3s as normal, but the last argument is set to true to enable "only trim" mode.
  bool blnTemp = false; // Dump variable...
  extract_mp3s(strMP3Path, strTempStart, &blnTemp, &blnTemp, true);
}

void append_mp3(string strDestFile, string strSourceFile) {
  // Additional check: There must not be any spaces in the source or dest filenames!
  // - This confuses the qmp3join utility because the command-line is also separated by spaces.
  if ((strDestFile.find(" ", 0) != strDestFile.npos) ||
      (strSourceFile.find(" ", 0) != strSourceFile.npos)) {
    my_throw("Cannot process MP3s with spaces in their name or path");
  }

  // Does the destination file exist?
  if (file_exists(strDestFile)) {
    // The destination file for the append exists. Attempt a quelcom mp3 append operation.
    string strCommand = "qmp3join -f " + strDestFile + " " + strSourceFile + " &> /dev/null";
    string strCmdOut = "";
    if (system_capture_out(strCommand, strCmdOut) != 0) {
      my_throw("Could not append MP3. " + strCmdOut + ". The command that failed: " + strCommand);
    }
  }
  else {
    // The destination file for the append does not exist.
    //   - Attempt to copy the source file to the destination.
   cp(strSourceFile, strDestFile);
  }
}

void check_mp3(const string & strMP3) {
  // Shell the "checkmp3" utility, throw an exception if the MP3 check fails!
  // - Check if the file exists:
  if (!file_exists(strMP3)) {
    my_throw("File not found: " + strMP3);
  }

  // Check if the mp3check tool is installed:
  if (!file_exists("/usr/bin/mp3check")) {
    my_throw("mp3check is not installed! Please install the \"mp3check\" debian package.");
  }

  // Prepare a command to shell mp3check:
  // The arguments to mp3check:
  // -B - ignore bitrate switching and enable VBR (variable bitrate) support.
  // -E - ignore junk after last frame (LYRICS TAG)
  // -S - ignore junk before first frame
  // -G - ignore 128 byte TAG after last frame
  // -W - ignore switching of constant parameters, such as sampling frequency.
  // -T - Ignore truncated last frmaes
  // -e - error check
  // -m 1 - print only 1 error

  // -s - Print one line per file and message instead of splitting into several lines

  string strcmd = "/usr/bin/mp3check -B -E -S -G -W -T -e -m 1 -s \"" + strMP3 + "\"";
  string strcmd_out = "";

  // Shell the command:
  int intret = system_capture_out(strcmd, strcmd_out);

  // Is there a non-zero return value, or something returned on the command-line?
  if (intret != 0 || strcmd_out != "") {
    // An mp3check error occured! Throw an exception!
    my_throw("mp3check: " + replace(strcmd_out, "\n", ". "));
  }
}

bool strcmpn (char *str1, char *str2, u_int32_t n) {
  // Compare the bytes of two strings for a certain length.
  // - This is a case-sensitive check.
  while (n && (*str1==*str2)) {
    str1++;
    str2++;
    n--;
  }
  return n==0;
}

bool RemoveTagsFromBuffer(char ** buffer, long * lngbuff_len) {
  // buffer points to a block of data containing the contents of an MP3 file.
  // Remove ID3v1 tags  (128 bytes at the end), LYRICS tags (also at the end)
  // and ID3v2+ tags (data at the start of the file)

  // Firstly an automatic error if the MP3 is smaller than 1 frame
  if (*lngbuff_len < lngFrameLength) {
    my_throw("MP3 is too small!");
  } // end if

  bool blnTagRemoved = false; // If this gets set to True then the tag scan will start again.
  do {
    // We loop here and continually remove tags until none are left, because the different tagging
    // programs used at Radio Retail don't know about all the tag types (read: LYRICS) and so
    // the 128 id3v1 tag can be before and after the LYRICS tag at the end of the MP3. This
    // will definitely confuse the Quelcom MP3 tools!
    blnTagRemoved = false; // Reset this variable at each iteration.

    // ************** Check for ID3v2 ***************
    // Information about ID3v2+ tags found here:
    //   http://www.id3.org/id3v2.3.0.txt and here:  http://www.id3.org/id3v2.4.0-structure.txt

    struct ID3v2_tag {
      char             ID3[3];  // Will contain 'ID3"
      unsigned int MajorVer:8; // Not interested  - Less than $FF
      unsigned int MinorVer:8; // Not interested - Less than $FF
      unsigned char Flags; // ID3 flags. Not interested.
      unsigned char SyncSafe_Size[4];  // Size of the entire ID3v2 frame. All bytes are < $ 80
                                                            // In each byte the first bit is 0, last 7 bits are used for
                                                            // the value.
    } __attribute__ ((packed)); // end struct

    //  __attribute__ ((packed)) -- used so that these structures are not word-aligned
    // - the structure is meant to take up only 10 bytes.

    // Get an ID3v2+ structure pointing to the start of the buffer...
    ID3v2_tag * id3v2_tag = (ID3v2_tag *) *buffer;

    if ((id3v2_tag->ID3[0] == 'I') &&
        (id3v2_tag->ID3[1] == 'D') &&
        (id3v2_tag->ID3[2] == '3') &&
        (id3v2_tag->SyncSafe_Size[0] < 128) &&
        (id3v2_tag->SyncSafe_Size[1] < 128) &&
        (id3v2_tag->SyncSafe_Size[2] < 128) &&
        (id3v2_tag->SyncSafe_Size[3] < 128)) {
      // The MP3 data starts with a ID3v2 frame.

      // Now calculate the size of the entire ID3v2 frame (including this ID3 header)
      int lngFrameSize=0;
      for (int i=0; i<4;++i) {
        // Add up all the groupings of 7 bits...
        lngFrameSize <<= 7; // Shift all bits 7 to the left.
        lngFrameSize += id3v2_tag->SyncSafe_Size[i]; // Add the next 7 bits ...
      } // end for
      lngFrameSize +=10; // Include the size of the frame header also...

      // Now delete the ID3v2 frame from the buffer...
      *lngbuff_len -= lngFrameSize;
      memmove(*buffer, (*buffer)+lngFrameSize, *lngbuff_len);

      char * NewPtr = (char *) realloc((*buffer), *lngbuff_len);
      if (NewPtr==NULL) {
        // realloc failed.
        my_throw("Could not remove an ID3v2 tag from an MP3!");
      } // end if

      // The re-alloc operation was successful so assign the new buffer pointer
      // (we're reducing the buffer size not increasing it, but this is still correct)
      *buffer=NewPtr;
      blnTagRemoved = true; // A tag was removed in the loop so loop again
    } // end if

    // ************** Check for ID3v1 (128 bytes at the end) ***************

    if (strcmpn ((*buffer + *lngbuff_len -128), (char *) "TAG", 3)) {
      // ID3v1 tag found. Cut it out.
      *lngbuff_len -= 128;
      char * NewPtr = (char *) realloc((*buffer), *lngbuff_len);
      if (NewPtr==NULL) {
        // realloc failed.
        my_throw("Could not remove an ID3v1 tag from an MP3!");
        return false;
      } // end if

      // The re-alloc operation was successful so assign the new buffer pointer
      // (we're reducing the buffer size not increasing it, but this is still correct)
      *buffer=NewPtr;
      blnTagRemoved = true; // A tag was removed in the loop so loop again
    } // end if

    // ************** Check for LYRICS tag at the end of the MP3 data ***************

    string strLyricsTagMarker = "LYRICSBEGIN";
    const char * chLyricsTagMarker = strLyricsTagMarker.c_str();
    int intLyricsTagMarkerLen = strLyricsTagMarker.length();

    // Start the backwards search for "LYRICSBEGIN" at the end of the MP3 data...
    long lngBackSearchStart = *lngbuff_len - intLyricsTagMarkerLen;

    // Search backwards through the buffer for the lyrics tag marker...
    long lngLyricsTagStart=-1; // The position in the mp3 buffer where the search string was found.
    for (long lngPos=lngBackSearchStart; lngPos >=0 && lngLyricsTagStart==-1; --lngPos) {
      long lngComparePos;
      for (lngComparePos=0;
            (lngComparePos<intLyricsTagMarkerLen) &&
            ((*buffer)[lngPos+lngComparePos]==chLyricsTagMarker[lngComparePos]);
            ++lngComparePos); // The for loop does all the work...
      if (lngComparePos >= intLyricsTagMarkerLen) {
        lngLyricsTagStart=lngPos;
      } // end if
    } // end for

    // Was LYRICSBEGIN found?
    if (lngLyricsTagStart != -1) {
      // Lyrics tag found at the very start of the file?
      if (lngLyricsTagStart == 0) my_throw("LYRICS tag found at the start of the MP3!");

      // Remove the LYRICS tag from the buffer.

      *lngbuff_len = lngLyricsTagStart; // No "off-by-one" errors here...
      char * NewPtr = (char *) realloc((*buffer), *lngbuff_len);
      if (NewPtr==NULL) {
        // realloc failed.
        my_throw("Could not remove a LYRICS tag from an MP3!");
        return false;
      } // end if

      // The re-alloc operation was successful so assign the new buffer pointer
      // (we're reducing the buffer size not increasing it, but this is still correct)
      *buffer=NewPtr;
      blnTagRemoved = true; // A tag was removed in the loop so loop again
    } // end if
  } while (blnTagRemoved); // Loop until the loop finds no more tags to be removed from the buffer...

  // Execution successfully reached this point, so quit with success
  return true;
} // end function

void mp3_strip_tags(const string & strMP3Path) {
  // Read the mp3 data into a buffer, scan the buffer for the start of tag data, and then
  // truncate the mp3 file so as not to include this tag data.

  // First remove any mp3gain tag info from the file:
  string strdummy;
  system_capture_out_throw("mp3gain -s d \"" + strMP3Path + "\"", strdummy);

  // Load the buffer into memory.
  long lngsize  = -1;
  char * buffer = NULL;
  load_buffer(strMP3Path, buffer, lngsize);

  // Now start processing the buffer.
  // - This procedure strips all tag data and replaces
  // the parameter variables with a pointer to the new file....
  try {
    RemoveTagsFromBuffer(&buffer, &lngsize);
    save_buffer(buffer, lngsize, strMP3Path);
    free(buffer);
  }
  catch(...) {
    free(buffer); // Free the buffer.
    throw; // Rethrow the exception.
  }
} // end function

// Some utility functions, wrapping mp3info:
int get_mp3_length(const string & strfile) {
  // Return the length in seconds
  string stroutput;
  system_capture_out_throw("/usr/bin/mp3info -p %S " + string_to_unix_filename(strfile), stroutput);
  if (!isint(stroutput)) my_throw("Invalid output from mp3info! " + stroutput);
  return strtoi(stroutput);
}

int get_mp3_bitrate(const string & strfile) {
  // Return the bitrate in kbps
  string stroutput;
  system_capture_out_throw("/usr/bin/mp3info -r m -p %r " + string_to_unix_filename(strfile), stroutput);
  if (!isint(stroutput)) my_throw("Invalid output from mp3info! " + stroutput);
  return strtoi(stroutput);
}

void mp3gain(const string & strMP3) {
  // Attempt to run mp3gain on an MP3

  // Run mp3gain on the file:
  string strmp3gain_cmd = "/usr/bin/mp3gain -r -k -q -f " +
      string_to_unix_filename(strMP3);
  string stroutput = "";
  system_capture_out_throw(strmp3gain_cmd, stroutput);

  // Check the output (mp3gain doesn't return an error code):
  string_splitter split(stroutput, "\n");

  // Fetch the last line of the output:
  string strline = split[split.size() - 1];

   // If the last line is "Not enough samples in <file> to do analysis", then
   // sometimes the mp3 is still ok, but mp3gain is confused.
   // We could fix the problem by converting to WAV and back to MP3 again, but all
   // MP3 tag details would be lost in the process!

  // Check if the last line is okay:
  if (!((substr(strline, 0, 27) == "Applying mp3 gain change of") ||
      (substr(strline, 0, 53) == "...but tag needs update: Writing tag information for ") ||
      ((substr(strline, 0, 13) == "No changes to") &&
      (right(strline, 13) == "are necessary")))) my_throw("mp3gain error! Output follows:\n" + stroutput);
}

void reencode_mp3(const string & strMP3) {
  // Convert an MP3 file to WAV and back again.
  // This helps with pushing regain header data (from mp3gain) into the sample
  // data useful for the local compile
  // - WARNING: All tag details are lost after the conversion. Also, silences
  //            marked by Cooledit can no longer be found after this process

  // Get the original bitrate before encoding to WAV:
  int intold_bitrate = get_mp3_bitrate(strMP3); // Bitrate before conversion to wav and back

  // Create the WAV file in a temporary directory:
  temp_dir wave_dir("normalise_wave");
  string strtemp_wav = (string)wave_dir + "temp.wav";
  {
    string strcmd = "/usr/bin/mpg123 --stereo -q -w " +
        string_to_unix_filename(strtemp_wav) + " " +
        string_to_unix_filename(strMP3);
    string stroutput;
    system_capture_out_throw(strcmd, stroutput);
    if (stroutput != "") my_throw("mpg321 error! Output follows:\n" + stroutput);
  }

  // Encode back to a temporary MP3:
  string strtemp_mp3 = (string)wave_dir + "temp.mp3";
  {
    string strcmd = "/usr/local/bin/bladeenc -quiet " +
        string_to_unix_filename(strtemp_wav) + " " +
        string_to_unix_filename(strtemp_mp3);
    string stroutput;
    int intret = system_capture_out(strcmd, stroutput);
    if (intret != 0 || stroutput != "")
        my_throw("bladeenc error! Output follows:\n" + stroutput);
  }

  // Get the new MP3 bitrate:
  int intnew_bitrate = get_mp3_bitrate(strtemp_mp3);

  // Check that the new bitrate is the same as the original:
  if (intnew_bitrate != intold_bitrate)
    my_throw("Bitrate before MP3->WAV->MP3 conversion (" + itostr(intold_bitrate) +
        ") is different to the bitrate afterwards (" + itostr(intnew_bitrate) + ")!");

  // Bitrate is the same before and afterwards. Move the temporary mp3 file
  // to the real mp3:
  mv(strtemp_mp3, strMP3);
}

string get_mp3_lyrics_tag(const string & strmp3) {
  // Does the MP3 exist?
  if (!file_exists(strmp3)) my_throw("File not found: " + strmp3);

  char * buffer = NULL;

  try {
    long lngbuff_len = -1;

    // Load just the last 10kb of the file into the buffer, not the entire file!
    {
      // Open the file
      FILE * stream = fopen(strmp3.c_str(), "rb");
      if (stream == NULL) my_throw("Could not open file! -> " + strmp3);

      // Fetch the file size.
      struct stat file_stats;
      if (fstat(fileno (stream), &file_stats) != 0) my_throw("File stats not retrieved!");
      unsigned long lngfile_size = file_stats.st_size;

      // The size of the block to read from the end of the file:
      unsigned long lngto_read = 10*1024;
      if (lngto_read > lngfile_size) lngto_read = lngfile_size;

      // Seek to the start:
      if (fseek(stream, lngfile_size - lngto_read, SEEK_SET) != 0) my_throw("Seek failed!");

      // Allocate memory:
      buffer = (char*)malloc(lngto_read);
      if (buffer == NULL) my_throw("Allocation failed!");
      lngbuff_len = lngto_read; // Setup a variable used later in the function...

      // Read:
      if (fread(buffer, 1, lngto_read, stream) != lngto_read) LOGIC_ERROR;

      // Close:
      fclose(stream);
    }

    long lngLyricsTagStart = buffer_search_backwards(buffer, lngbuff_len, "LYRICSBEGIN");
    if (lngLyricsTagStart < 0) my_throw("Could not find LYRICSBEGIN in " + strmp3 + "!");

    // Move the start pos to after LYRICSBEGIN:
    lngLyricsTagStart += 11;

    long lngLyricsTagEnd = buffer_search_forwards(buffer+lngLyricsTagStart, lngbuff_len-lngLyricsTagStart, "LYRICS200");
    if (lngLyricsTagEnd < 0) my_throw("Could not find LYRICS200 in " + strmp3 + "!");
    lngLyricsTagEnd += lngLyricsTagStart;

    // We found LYRICS200. Move the tag end backwards 6 chars (get rid of some tag-related length info)
    lngLyricsTagEnd -= 7;

    // Now extract the string:
    {
      int inttag_buff_size=lngLyricsTagEnd - lngLyricsTagStart + 2;
      char * chret = (char *) malloc(inttag_buff_size);

      // Set the last char to 0:
      chret[inttag_buff_size - 1] = 0;

      // Copy over the part of the buffer containing the lyrics tag:
      memcpy(chret, buffer + lngLyricsTagStart, inttag_buff_size - 1);

      // Fetch the string to return:
      string strret = chret;

      // Free buffers:
      free(buffer);
      free(chret);

      // Return the string:
      return strret;
    }
  } catch (...) {
    if (buffer != NULL) {
      free(buffer);
      buffer=NULL;
    }
    throw;
  }
}

#endif // END: #ifdef __linux__

