
#include "music_history.h"
#include "common/psql.h"
#include "common/my_string.h"

void music_history::load(pg_connection & db)
{
  // Load the most recent music history entries from the schedule database:
  ap_pg_result rs = db.exec("SELECT strfile FROM tblmusichistory WHERE strfile IS NOT NULL ORDER BY lngplayedmp3 DESC LIMIT " + itostr(max_history_length));

  while (*rs) {
    m_history.push_back(rs->field("strfile"));
    (*rs)++;
  }

  tidy(); // Clear out old history entries.
}

void music_history::song_played(pg_connection & db, const string & strfile, const string & strdescr)
{
  // Called when a song has started playing. Updates the music history.
  // Does not delete old music history entries. The database maintenance
  // script now does that.
  song_played_no_db(strfile, strdescr);

  // Store the song in tblmusichistory:
  db.exec("INSERT INTO tblmusichistory (dtmtime, strdescription, strfile) VALUES ("
          + psql_now + ", " + psql_str(strdescr) + ", " + psql_str(strfile) + ")");
}

void music_history::song_played_no_db(const string & strfile, const string & strdescr)
{
  // This function is used when building a playlist, to "simulate" the "don't repeat recent
  // music" effect
  m_history.push_front(strfile);
  tidy(); // Delete old entries
}

bool music_history::song_played_recently(const string & strfile, const int count)
{
  auto i = m_history.begin();
  int c = 0; // How many items we have iterated over;
  bool found = false; // Set to true if we find the song
  while (i != m_history.end() && c < count) {
    if (*i == strfile) {
      found = true;
      break;
    }
    i++; c++;
  }
  return found;
}

/// Did a song by the specified artist play within the most recent X songs?
bool music_history::artist_song_played_recently(const std::string & strartist,
                                                const int count,
                                                mp3_tags & mp3tags) {
  auto i = m_history.begin();
  int c = 0; // How many items we have iterated over;
  bool found = false; // Set to true if we find the song
  while (i != m_history.end() && c < count) {
    string item_artist = mp3tags.get_mp3_artist(*i);
    if (item_artist == strartist) {
      found = true;
      break;
    }
    i++; c++;
  }
  return found;
}


void music_history::clear() {
  // Clear the in-memory history (not the database table)
  m_history.clear();
}

const std::list<std::string> music_history::get_history() const {
  // Fetch a read-only copy of the music history list
  // (newer entries are at the front of the queue)
  return m_history;
}

void music_history::tidy()
{
  // Clear out the oldest history entries:
  while (m_history.size() > max_history_length)
    m_history.pop_back();
}
