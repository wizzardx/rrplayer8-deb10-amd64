
// test_player_get_next_item.h

// Tests for the logic found in src/player_get_next_item.cpp

#include <cxxtest/TestSuite.h>

#include "player.h"
#include "common/exception.h"
#include "common/my_string.h"

// Utility functions

// Tests for get_next_ok_music_item()

class TestGetNextOkMusicItem : public CxxTest::TestSuite {
public:
    // Helper type definitions
    typedef auto_ptr<pg_transaction> ap_pg_transaction;

    // Helper classes

    // A version of the segment class with updated methods to assist the
    // tests.
    class patched_segment : public segment {

    public:

        // How many times get_next_item() has been called with the blnasap
        // argument set to true
        int get_next_item_blnasap_count;

        // Constructor
        patched_segment() {
            get_next_item_blnasap_count = 0;
        }

        // Version of get_next_item() that logs information about the
        // arguments
        virtual void get_next_item(programming_element & pe, pg_conn_exec & db,
                                   const int intstarts_ms,
                                   const player_config & config,
                                   mp3_tags & mp3tags,
                                   const music_history & musichistory,
                                   const bool blnasap) {
            if (blnasap) {
                get_next_item_blnasap_count += 1;
            }
            segment::get_next_item(pe, db, intstarts_ms, config, mp3tags,
                                   musichistory, blnasap);
        }
    };

    // Main objects passed to get_next_ok_music_item()
    int intstarts_ms;
    programming_element next_item;

    // Other objects passed to get_next_ok_music_item(), providing data it
    // needs to operate
    music_history mhistory;
    mp3_tags mp3tags;
    player_config config;
    player_run_data run_data;
    ap_pg_transaction trans;

    // A pointer to the mock segment we setup in the constructor:
    patched_segment * ppatched_segment;

    // Per-test fixture setup
    void setUp() {
        // Setup objects to be passed to get_next_ok_music_item()
        intstarts_ms = 5000;

        // Replace run_data.current_segment with a mocked version which
        // provides more information for testing
        ppatched_segment = new patched_segment();
        run_data.current_segment = ap_segment(ppatched_segment);

        // Create a programming element object
        programming_element pe;
        pe.cat = SCAT_MUSIC;
        pe.strmedia = "/dir/to/music/mp3s/test.mp3";
        pe.blnloaded = true;
        programming_element_list pel;
        pel.push_back(pe);
        run_data.current_segment->set_pel(pel);

        // Add a single programming element to the segment.
        run_data.current_segment->programming_elements.push_back(pe);
        run_data.current_segment->blnloaded = true;

        // Setup a database transaction, with testing data
        pg_connection db;
        db.open("dbname=schedule_test");
        trans = ap_pg_transaction(new pg_transaction(db));
        _setup_test_db_data(*trans);
    }

    // Per-test fixture teardown
    void tearDown() {
        mhistory.clear();
        run_data.current_segment->reset();
        trans->abort();
        logging.remove_all_loggers();
    }

    // A basic "smoke test" for get_next_ok_music_item()
    void test_should_work() {
        // Run the tested function
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               *trans, config, run_data);

        // Check the output of the tested function
        TS_ASSERT(next_item.blnloaded);
        TS_ASSERT_EQUALS(next_item.cat, SCAT_MUSIC);
        TS_ASSERT_EQUALS(next_item.strmedia, "/dir/to/music/mp3s/test.mp3");
    }

    // For longer playlists (>=50 items), the method should attempt
    // up to (playlist length * 2) times to find an ok music item.
    void test_should_attempt_correct_number_of_times_for_long_playlists() {
        // Put the same programming element into the music history and
        // the segment 100 times:
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 99; ++i) {
            programming_element pe;
            pe.cat = SCAT_MUSIC;
            pe.strmedia = "/dir/to/music/mp3s/test.mp3";
            pe.blnloaded = true;
            pel.push_back(pe);
            mhistory.song_played_no_db(pe.strmedia, "<song description>");
        }
        run_data.current_segment->set_pel(pel);
        run_data.current_segment->blnrepeat = true;
        run_data.current_segment->blnloaded = true;

        // Add a logger callback function, to suppress the debug and warning
        // messages that will be logged (skipping song and file not found)
        logging.add_logger(_null_logger);

        // Test get_next_ok_music_item()
        string expected_error = "I was unable to find a song which has "
                                "not been played recently!";
        TS_ASSERT_THROWS_EQUALS (
            get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                                   *trans, config, run_data),
            const my_exception &e, e.get_error(), expected_error
        );
        TS_ASSERT_EQUALS(run_data.current_segment->get_num_fetched(), 200);
    }

    // For shorter playlists (< 50 items), the method should attempt
    // up to 100 times to find an OK music item.
    void test_should_attempt_correct_number_of_times_for_short_playlists() {
        // Put the same programming element into the music history and
        // the segment 20 times:
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 19; ++i) {
            programming_element pe;
            pe.cat = SCAT_MUSIC;
            pe.strmedia = "/dir/to/music/mp3s/test.mp3";
            pe.blnloaded = true;
            pel.push_back(pe);
            mhistory.song_played_no_db(pe.strmedia, "<song description>");
        }
        run_data.current_segment->set_pel(pel);
        run_data.current_segment->blnrepeat = true;
        run_data.current_segment->blnloaded = true;

        // Add a logger callback function, to suppress the debug and warning
        // messages that will be logged (skipping song and file not found)
        logging.add_logger(_null_logger);

        // Test get_next_ok_music_item()
        string expected_error = "I was unable to find a song which has "
                                "not been played recently!";
        TS_ASSERT_THROWS_EQUALS (
            get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                                   *trans, config, run_data),
            const my_exception &e, e.get_error(), expected_error
        );
        TS_ASSERT_EQUALS(run_data.current_segment->get_num_fetched(), 100);
    }

    // Should use 'get the next item ASAP' logic (cache) if there are less than
    // 5 seconds remaining until the next item needs to start playing.
    void test_should_use_asap_logic_when_less_than_5_seconds() {
        // Run the tested function
        intstarts_ms = 4999;
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               *trans, config, run_data);

        // Check the output of the tested function
        TS_ASSERT(next_item.blnloaded);
        TS_ASSERT_EQUALS(next_item.cat, SCAT_MUSIC);
        TS_ASSERT_EQUALS(next_item.strmedia, "/dir/to/music/mp3s/test.mp3");

        // Check that segment::get_next_item() was called once with
        // blnasap set to True:
        TS_ASSERT_EQUALS(ppatched_segment->get_next_item_blnasap_count, 1);
    }

    // Should not use 'get the next item ASAP' logic (cache) if there are more
    // than 5 seconds remaining until the next item needs to start playing.
    void test_should_not_use_asap_logic_when_more_than_5_seconds() {
        // Run the tested function
        intstarts_ms = 5000;
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               *trans, config, run_data);

        // Check the output of the tested function
        TS_ASSERT(next_item.blnloaded);
        TS_ASSERT_EQUALS(next_item.cat, SCAT_MUSIC);
        TS_ASSERT_EQUALS(next_item.strmedia, "/dir/to/music/mp3s/test.mp3");

        // Check that segment::get_next_item() was not called with blnasap set
        // to true.
        TS_ASSERT_EQUALS(ppatched_segment->get_next_item_blnasap_count, 0);
    }

    // Helper methods

    // Setup testing db data
    void _setup_test_db_data(pg_conn_exec & db) {
        // Insert a new record into tblinstore_media_dir
        string sql = "SELECT nextval('tblinstore_media_dir_lnginstore_media_"
                     "dir_seq')";
        long lnginstore_media_dir = strtoi(db.exec(sql).field("nextval"));
        sql = "INSERT INTO tblinstore_media_dir (lnginstore_media_dir, strdir)"
              " VALUES (?, ?)";
        pg_params params = ARGS_TO_PG_PARAMS(ltostr(lnginstore_media_dir),
                                             psql_str("/dir/to/music/mp3s/"));
        db.exec(sql, params);
        // Insert a record for "test.mp3" into tblinstore_media
        sql = "INSERT INTO tblinstore_media (lnginstore_media_dir, strfile, "
                                            "intlength_ms, "
                                            "blndynamically_compressed) "
              "VALUES (?, ?, ?, ?)";
        params = ARGS_TO_PG_PARAMS(ltostr(lnginstore_media_dir),
                                   psql_str("test.mp3"),
                                   "60000", psql_bool(true));
        db.exec(sql, params);
    }

    // Logging function that does nothing. Used to suppress logger output
    static void _null_logger(const log_info & LI) {}
};
