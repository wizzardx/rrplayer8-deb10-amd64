
// test_player_get_next_item.h

// Tests for the logic found in src/player_get_next_item.cpp

#include <cxxtest/TestSuite.h>

#include "player.h"
#include "common/exception.h"
#include "common/my_string.h"

// Namespace containing utilities used by the tests in this header
namespace test_player_get_next_item {


    // Storage for logging calls to patched methods
    class call {
    public:
        string func; // Name of the function
        string args; // String describing the arguments.
        call(const string & func, const string & args) : func(func), args(args) {}

        // Return a user-friendly string representation of the object
        string str() {
            string ret;
            ret = "call(" + func;
            if (args != "") {
                ret += ", " + args;
            }
            ret += ")";
            return ret;
        }

    };


    typedef vector<call> calls_list;
    calls_list calls;

    // Patched classes


    // A version of the segment class with updated methods to assist the
    // tests.
    class patched_segment : public segment {

    public:

        // How many times get_next_item() has been called with the blnasap
        // argument set to true
        int get_next_item_blnasap_count;

        // Set this to true to cause the patched 'get_next_item()' method to
        // reload the segment's playlist
        bool update_playlist_during_get_next_item;

        // Constructor
        patched_segment() {
            get_next_item_blnasap_count = 0;
            update_playlist_during_get_next_item = false;
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
            if (update_playlist_during_get_next_item) {
                // Replace the playlist with one where the testing item is
                // listed 60 times

                // Add the testing mp3 to the playlist and history 60 times
                programming_element_list pel;
                for (int i = 0; i <= 59; ++i) {
                    programming_element pe;
                    pe.cat = SCAT_MUSIC;
                    pe.strmedia = "/dir/to/music/mp3s/test.mp3";
                    pe.blnloaded = true;
                    pel.push_back(pe);
                }
                set_pel(pel);
                blnrepeat = true;
                blnloaded = true;
                // Don't update again:
                update_playlist_during_get_next_item = false;
            }

            segment::get_next_item(pe, db, intstarts_ms, config, mp3tags,
                                   musichistory, blnasap);
        }
    };


    // Patched music history class, for easier testing
    class patched_music_history: public music_history {
    public:

        virtual bool song_played_recently(const std::string & strfile,
                                          const int count) {

            // Log the call and the args:
            // Save the song_played_recently() arguments
            calls.push_back(call("song_played_recently",
                            strfile + ", " + itostr(count)));
            // Call the parent classes method
            return music_history::song_played_recently(strfile, count);
        }

        virtual void clear() {
            music_history::clear();
        }
    };


    // Setup testing db data
    void setup_test_db_data(pg_conn_exec & db) {
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

    // Logging function that discards logged messages. Used to suppress
    // logged messages
    static void null_logger(const log_info & LI) {}
}

// Tests for get_next_ok_music_item()

class TestGetNextOkMusicItem : public CxxTest::TestSuite {
public:
    // Helper type definitions
    typedef auto_ptr<pg_transaction> ap_pg_transaction;

    // Main objects passed to get_next_ok_music_item()
    int intstarts_ms;
    programming_element next_item;

    // Other objects passed to get_next_ok_music_item(), providing data it
    // needs to operate
    test_player_get_next_item::patched_music_history mhistory;
    mp3_tags mp3tags;
    player_config config;
    player_run_data run_data;
    ap_pg_transaction trans;

    // A pointer to the mock segment we setup in the constructor:
    test_player_get_next_item::patched_segment * ppatched_segment;

    // Per-test fixture setup
    void setUp() {
        using namespace test_player_get_next_item;
        // Clear call log
        calls.clear();

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
        setup_test_db_data(*trans);
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
        using namespace test_player_get_next_item;
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
        logging.add_logger(null_logger);

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
        using namespace test_player_get_next_item;
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
        logging.add_logger(null_logger);

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

    // Should work correctly if the underlying playlist changes while
    // checking items.
    void test_should_correctly_handle_playlist_updates_during_loop() {
        using namespace test_player_get_next_item;
        // Setup the patched segment so that it will update it's playlist
        // during the call to get_next_item()
        ppatched_segment->update_playlist_during_get_next_item = true;

        // Add the testing mp3 to the playlist and history 10 times
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 9; ++i) {
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

        // Update the time the programming element list was changed, so that
        // when the list is updated by get_next_item(),
        // get_next_ok_music_item() will be able to detect it.
        ppatched_segment->dtmpel_updated = -1;

        // Add a logger callback function, to suppress the debug and warning
        // messages that will be logged (skipping song and file not found)
        logging.add_logger(null_logger);

        // Run the tested method
        string expected_error = "I was unable to find a song which has "
                                "not been played recently!";
        TS_ASSERT_THROWS_EQUALS (
            get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                                   *trans, config, run_data),
            const my_exception &e, e.get_error(), expected_error
        );

        // Check the output of the tested function
        TS_ASSERT(next_item.blnloaded);
        TS_ASSERT_EQUALS(next_item.cat, SCAT_MUSIC);
        TS_ASSERT_EQUALS(next_item.strmedia, "/dir/to/music/mp3s/test.mp3");

        // Check that the 'count' figure passed to
        // music_history::song_played_recently() was correct
        // Should be 45 (75% of 60)
        //  - patched segment::get_next_item puts 60 items in the playlist.
        //  - Player uses 75% of the playlist length as a limit for
        //    repetition.
        TS_ASSERT_EQUALS(calls.size(), 100);
        string expected_args = "/dir/to/music/mp3s/test.mp3, 45";
        calls_list::iterator it = calls.begin();
        while(it != calls.end()) {
            call c = *it;
            TS_ASSERT_EQUALS(c.args, expected_args);
            ++it;
        }
    }

    // Should return any non-music item that is returned while scanning items.
    void test_should_return_any_non_music_item() {
        using namespace test_player_get_next_item;
        // Add 100 elements to the playlist and history. Most of them are music
        // except for the 50th one
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 99; ++i) {
            programming_element pe;
            if (i == 49) {
                // The 50th item is non-music
                pe.cat = SCAT_PROMOS;
                pe.strmedia = "/dir/to/announcement/mp3s/ann.mp3";
            }
            else {
                // All others are music
                pe.cat = SCAT_MUSIC;
                pe.strmedia = "/dir/to/music/mp3s/test.mp3";
            }
            pe.blnloaded = true;
            pel.push_back(pe);
            mhistory.song_played_no_db(pe.strmedia, "<song description>");
        }
        run_data.current_segment->set_pel(pel);
        run_data.current_segment->blnrepeat = true;
        run_data.current_segment->blnloaded = true;

        // Add a logger callback function, to suppress the debug and warning
        // messages that will be logged (skipping song and file not found)
        logging.add_logger(null_logger);

        // Get the next ok music item:
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               *trans, config, run_data);

        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/announcement/mp3s/ann.mp3");
        TS_ASSERT_EQUALS(next_item.cat, SCAT_PROMOS);

    }
};
