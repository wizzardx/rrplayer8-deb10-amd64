
#include <cxxtest/TestSuite.h>

#include "common/exception.h"

#include "music_history.h"

namespace test_music_history {
    // "Patched" version of the mp3_tags class, to allow easier testing
    class patched_mp3_tags : public mp3_tags {
    public:
        typedef map <const string, string> string_to_string_map;
        string_to_string_map mock_mp3_artists;

        // Clear members to initial values
        void clear() {
            mock_mp3_artists.clear();
        }

        // Replacement for get_mp3_description(), to make tests easier
        virtual string get_mp3_description(const string & strFilePath) {
            return "<MP3 Description>";
        }

        // Replacement for get_mp3_artist(), to make tests easier
        virtual string get_mp3_artist(const string & strFilePath) {
            string_to_string_map::const_iterator it =
                mock_mp3_artists.find(strFilePath);
            if (it == mock_mp3_artists.end()) {
                my_throw("No artist for " + strFilePath + " found in mock");
            }
            return it->second;
        }
    };
}

class TestMusicHistory : public CxxTest::TestSuite
{
public:
    // Tests for song_artist_played_recently()

    music_history mhistory;
    test_music_history::patched_mp3_tags mp3tags;

    void setUp() {
        // Setup vars
        mhistory.clear();
        mhistory.song_played_no_db("/mp3s/dir/1.mp3", "<desc>");
        mhistory.song_played_no_db("/mp3s/dir/2.mp3", "<desc>");
        mhistory.song_played_no_db("/mp3s/dir/3.mp3", "<desc>");
        mp3tags.clear();
        mp3tags.mock_mp3_artists["/mp3s/dir/1.mp3"] = "Artist 1";
        mp3tags.mock_mp3_artists["/mp3s/dir/2.mp3"] = "Artist 2";
        mp3tags.mock_mp3_artists["/mp3s/dir/3.mp3"] = "Artist 3";
    }

    // Should return 'false' when artist hasn't played recently
    void test_song_artist_played_recently_should_return_false_when_expected() {
        // Test function
        TS_ASSERT_EQUALS(mhistory.artist_song_played_recently(
            "Artist 1", 2, mp3tags), false);
    }

    // Should return 'true' when artist hasn't played recently
    void test_artist_song_played_recently_should_return_true_when_expected() {
        // Test function
        TS_ASSERT_EQUALS(mhistory.artist_song_played_recently(
            "Artist 1", 3, mp3tags), true);
    }
};
