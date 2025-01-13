/// @file
/// Caching for mp3 tag details.
/// This class is used to quickly retrieve tag details for mp3s. It pre-reads details from a text file
/// and then uses this pre-read data when the program wants tag details. If tag details are wanted
/// which are not listed in memory, they are loaded from the mp3 and then stored in memory.
/// - Later the buffer textfile is re-written.

#ifndef MP3_TAGS_H
#define MP3_TAGS_H

using namespace std;

#include <string>
#include <map>

/// A private class for storing mp3 tag details in memory, for quick retrieval later.
struct tblmp3_info {
  string strMP3Path; ///< The path of the mp3 we are caching MP3 tag details for. Also the unique key.

  // The MP3 details we are interested in
  long lngFileSize; ///< Size in bytes
  string strArtist; ///< Song artist
  string strAlbum;  ///< Song album
  string strTrackName; ///< Song track name
  int intLength; ///< Song length in seconds
};
typedef map <const string, tblmp3_info> tblmp3_info_map;

// Now the main  mp3_tags class
class mp3_tags {
public:
  mp3_tags();
  virtual ~mp3_tags();

  void init(const string & strBuffFilePath);  ///< Initialize the mp3_tags object, provide a buffer filename
  virtual string get_mp3_description(const string & strFilePath);
  int get_mp3_length(const string & strFilePath); ///< Fetch the length of an mp3
  virtual string get_mp3_artist(const string & strFilePath); ///< Fetch the artist for an MP3
  string get_mp3_album(const string & strFilePath); ///< Fetch the album for an MP3
  void save_changes(); ///< Rewrite the buffer file if there are any new tag details that have been loaded.

private:
  string strTagBufferFile;   ///< Filename to save and load buffered mp3 tag details from
  bool blnTagDetailsChanged; ///< Set to true if the tag buffer details change (new, removed, etc)

  // Internal caching functions
  tblmp3_info_map mp3_info; ///< Used to store the file path (as the key) and the mp3 description in memory

  // - So we can quickly find the description we're looking for.
  void cache_file_tag(const string & strFile, const long lngFileSize, const string & strArtist, const string & strAlbum, const string & strTrackName, const int intTrackLength);
  bool file_tag_cached(const string & strFile);
  tblmp3_info_map::const_iterator get_mp3_info_item(const string & strFilePath); /// Fetch the cached item. If it doesn't exist then load it
};

#endif
