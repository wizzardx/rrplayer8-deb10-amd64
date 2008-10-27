// test_segment.h

// Tests for the logic found in src/segment.cpp

#include <cxxtest/TestSuite.h>

#include "segment.h"
#include "common/my_string.h"

namespace test_segment {

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

// Tests for the segment class

class TestSegment : public CxxTest::TestSuite {
public:
    // Helper type definitions
    typedef auto_ptr<pg_transaction> ap_pg_transaction;

    // Members
    test_segment::patched_mp3_tags mp3tags;
    music_history mhistory;
    player_config config;
    ap_pg_transaction trans;

    // Per-test fixture setup
    void setUp() {
        pg_connection db;
        db.open("dbname=schedule_test");
        trans = ap_pg_transaction(new pg_transaction(db));
        _setup_test_db_data(*trans);
        mp3tags.clear();
    }

    // Per-test fixture teardown
    void tearDown() {
        trans->abort();
    }

    // segment::set_pel() should work correctly
    void test_set_pel_should_work() {
        // Declare variables
        programming_element_list pel;
        programming_element pe;
        segment seg;
        seg.blnloaded = true;

        // Update programming element list
        pe.cat = SCAT_MUSIC;
        pe.blnloaded = true;
        for (int i = 0; i <= 99; i++) {
            pe.strmedia = "/dir/to/music/mp3s/song" + itostr(i) + ".mp3";
            pel.push_back(pe);
        }

        // Get the time before setting the programming elements list
        datetime before = now();

        // Set the segment's programming element list
        seg.set_pel(pel);

        // Check the segment
        TS_ASSERT(seg.dtmpel_updated >= before);
        TS_ASSERT(seg.dtmpel_updated <= before + 1);

        pe.reset();

        // Fetch the items
        for (int i = 0; i <= 99; i++) {
            seg.get_next_item(pe, *trans, 5000, config, mp3tags, mhistory,
                              false);
            string expected_media = "/dir/to/music/mp3s/song" + itostr(i) +
                                    ".mp3";
            TS_ASSERT_EQUALS(pe.strmedia, expected_media);
        }
    }

    void test_count_remaining_playlist_artists_without_repeat_should_work() {
        // Declare variables
        programming_element_list pel;
        programming_element pe;
        segment seg;
        seg.blnloaded = true;

        // Update programming element list, and mock list of mp3 artists
        pe.cat = SCAT_MUSIC;
        pe.blnloaded = true;
        for (int i = 0; i <= 99; i++) {
            pe.strmedia = "/dir/to/music/mp3s/song" + itostr(i) + ".mp3";
            pel.push_back(pe);
            mp3tags.mock_mp3_artists[pe.strmedia] = "<Artist #" +
                                                    itostr(i/10) + ">";
        }
        seg.set_pel(pel);

        // Simulate fetching the first 30 items
        for (int i = 0; i <= 29; i++) {
            seg.get_next_item(pe, *trans, 5000, config, mp3tags, mhistory,
                              false);
        }

        // There should be a total of 7 unique artists. 0-3 were exhausted
        // by fetching the first 30.
        TS_ASSERT_EQUALS(seg.count_remaining_playlist_artists(mp3tags), 7);
    }

    void test_count_remaining_playlist_artists_with_repeat_should_work() {
        // Declare variables
        programming_element_list pel;
        programming_element pe;
        segment seg;
        seg.blnloaded = true;
        seg.blnrepeat = true; // Enable playlist repetition

        // Update programming element list, and mock list of mp3 artists
        pe.cat = SCAT_MUSIC;
        pe.blnloaded = true;
        for (int i = 0; i <= 99; i++) {
            pe.strmedia = "/dir/to/music/mp3s/song" + itostr(i) + ".mp3";
            pel.push_back(pe);
            mp3tags.mock_mp3_artists[pe.strmedia] = "<Artist #" +
                                                    itostr(i/10) + ">";
        }
        seg.set_pel(pel);

        // Simulate fetching the first 30 items
        for (int i = 0; i <= 29; i++) {
            seg.get_next_item(pe, *trans, 5000, config, mp3tags, mhistory,
                              false);
        }

        // Get the number of unique artists in the remaining playlist:
        // (Should be 10 total - the first 30 are included because the
        // playlist is setup to repeat)
        TS_ASSERT_EQUALS(seg.count_remaining_playlist_artists(mp3tags), 10);
    }

    // Helper methods

    void _setup_test_db_data(pg_conn_exec & db) {
        // Insert a new record into tblinstore_media_dir
        string sql = "SELECT nextval('tblinstore_media_dir_lnginstore_media_"
                     "dir_seq')";
        long lnginstore_media_dir = strtoi(db.exec(sql)->field("nextval"));
        sql = "INSERT INTO tblinstore_media_dir (lnginstore_media_dir, strdir)"
              " VALUES (?, ?)";
        pg_params params = ARGS_TO_PG_PARAMS(ltostr(lnginstore_media_dir),
                                             psql_str("/dir/to/music/mp3s/"));
        db.exec(sql, params);

        // Insert records for the songs
        sql = "INSERT INTO tblinstore_media (lnginstore_media_dir, strfile, "
                                            "intlength_ms, "
                                            "blndynamically_compressed) "
              "VALUES (?, ?, ?, ?)";
        for (int i = 0; i <= 99; i++) {
            string media = "song" + itostr(i) + ".mp3";
            params = ARGS_TO_PG_PARAMS(ltostr(lnginstore_media_dir),
                                    psql_str(media),
                                    "60000", psql_bool(true));
            db.exec(sql, params);
        }
    }
};
