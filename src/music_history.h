
#ifndef MUSIC_HISTORY_DP20060525_H
#define MUSIC_HISTORY_DP20060525_H

#include "common/mp3_tags.h"

#include <list>
#include <string>

// Forward declarations:
class pg_connection;

/// A sub-class to manage the players music history
/// (also to prevent the same songs from playing too soon
/// in succession)
class music_history
{
 public:
   /// Load music history from the schedule database
   void load(pg_connection & db);

   /// Called when a song has started playing. Updates the music history.
   void song_played(pg_connection & db, const std::string & strfile, const std::string & strdescr);

   /// Same as song_played(), but does not update the database
   void song_played_no_db(const std::string & strfile, const std::string & strdescr);

   /// Did this song play within the most recent X songs?
   virtual bool song_played_recently(const std::string & strfile, const int count);

   /// Did a song by the specified artist play within the most recent X songs?
   virtual bool artist_song_played_recently(const std::string & strartist, const int count, mp3_tags & mp3tags);

   /// Clear the in-memory music history (not the database table)
   virtual void clear();

   /// Fetch a read-only copy of the music history list
   /// (newer entries are at the front of the queue)
   const std::list<std::string> get_history() const;

 private:
   /// Maximum history entries to keep in memory
   static const unsigned int max_history_length = 1000;

   /// A list of the most recent music files played
   std::list<std::string> m_history;

   /// Clear out the oldest history entries
   void tidy();
};

#endif
