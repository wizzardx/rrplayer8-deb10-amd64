/// @file
/// A collection of miscellenious Radio Retail-specific logic.
/// Use this for useful RR logic which hasn't been sorted yet.
/// Logic inside this library will eventually be moved into other rr_ libraries.
/// - If you use this library, be prepared to update your includes in the future.

#ifndef RR_MISC_H
#define RR_MISC_H

#include <string>
#include "logging.h"

#ifdef RR_MISC_DB
#error Do not set RR_MISC_DB! (include common/rr_misc_db.h instead)
#endif

using namespace std;

/// Return true if the filename matches one of the listed prefixes (space-separated string).
/// For certain (hardcoded) prefixes (AD CA SP MU), chars after the first 2 are not checked.
/// eg: SPM, SPC are recognised as SPs. For all other prefixes, the non-alphabetic lead-in
/// part muct match. (eg: PC1A does *not* match "PC")
bool rr_media_prefix_matches(const string & strfile, const string & strprefix_list);

/// Convert the filename of an international prerec filename to the South African layout.
/// eg: PL1A000001.MP3 ->
///     prl0000001.mp3
/// The 2nd arg is set to PC1A, PL1A, etc.
string convert_international_prerec_filename_to_sa(const string & strfile);

/// Extract the prefix of an rr media filename. This is done by removing the numeric characters from the end
string get_rr_media_prefix(const string & strfile);

/// Standard RR logging (stdout, logfile, and month-day-based logfile rotation)
void rr_log_file(const log_info & LI, const string & strlog_file);

// Is the file a cd track? (as understood by XMMS)
bool file_is_cd_track(const string & strfile); // Return true if the filename is ends with .cda or .cdr

/// Write lines to the logfile listing that the program is starting, it's version, etc.
void rr_log_prog_starting(const bool blnfancy=true);

// A (non-rr-specific) set of macros to help run timed events at specified intervals:
// (still need to find a permanent home for these)

// Simple version:
#define RUN_TIMED(FUNC, FREQ) { \
  static datetime dtmlast = datetime_error; \
  datetime dtmnow = now(); \
  if (dtmlast/(FREQ) != dtmnow/(FREQ)) { \
    try { \
      FUNC; \
    } catch_exceptions; \
    dtmlast = now(); \
  } \
}

// And with a "cutoff" (ie, only run logic until a certain time):
#define RUN_TIMED_CUTOFF(FUNC, FREQ, CUTOFF) { \
  static datetime dtmlast = datetime_error; \
  if (now() < CUTOFF) { \
    datetime dtmnow = now(); \
    if (dtmlast/(FREQ) != dtmnow/(FREQ)) { \
      try { \
        FUNC; \
      } catch_exceptions; \
      dtmlast = now(); \
    } \
  } \
}

// Data validation:

#define NOT_EMPTY(field) check_not_empty(#field, field);
string check_not_empty(const string & strfield, const string & strvalue);
int    check_not_empty(const string & strfield, const int intvalue);
bool   isprice(const string & strprice);

#endif
