
// test_player_get_next_item.h

// Tests for the logic found in src/player_get_next_item.cpp

#include <cxxtest/TestSuite.h>

#include "player.h"
#include "common/exception.h"
#include "common/my_string.h"

#include <tr1/unordered_set>

// Namespace containing utilities used by the tests in this header
namespace test_player_get_next_item {

    // Storage for logging calls to patched methods
    class call {
    public:
        string func; // Name of the function
        string args; // String describing the arguments.
        call(const string & func, const string & args="") :
            func(func), args(args) {}

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

    typedef vector<call> call_list;
    call_list calls;

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

        // When this vector is set, it is used by
        // patched_music_history::song_played_recently(), instead of calling
        // music_history::song_played_recently()
        typedef tr1::unordered_set <string> string_set;
        string_set recently_played;

        virtual bool song_played_recently(const std::string & strfile,
                                          const int count) {

            // Log the call and the args:
            // Save the song_played_recently() arguments
            calls.push_back(call("song_played_recently",
                            strfile + ", " + itostr(count)));

            // Was patched_music_history::recently_played populated by a
            // test?
            if (recently_played.size() != 0) {
                // Yes. Return true if the song is listed in the vector
                string_set::const_iterator it = recently_played.find(strfile);
                return it != recently_played.end();
            }
            else {
                // No. Use the ancestor's method
                return music_history::song_played_recently(strfile, count);
            }
        }

        virtual void clear() {
            calls.push_back(call("music_history::clear"));
            recently_played.clear();
            music_history::clear();
        }
    };

    // "Patched" version of the mp3_tags class, to allow easier testing of
    // get_next_ok_music_item().
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

    class mock_pg_result : public pg_result {
    public:
        // Types
        typedef vector<string> columns_t;
        typedef vector<columns_t> rows_t;

        // Attributes
        columns_t columns;
        rows_t rows;

        // Return the value of a field on the current row
        string field(const string & strfield_name,
                     const char * strdefault_val = NULL) const {
            return rows.at(row_num).at(get_field_num(strfield_name));
        }

        virtual long size() const {
            return rows.size();
        }

    private:

        // Return the column number of the field.
        int get_field_num(const string & field_name) const {
            string field_lower = lcase(field_name);
            columns_t::const_iterator iter = columns.begin();
            int colnum = 0;
            while (iter != columns.end()) {
                if (lcase(*iter) == field_lower) {
                    return colnum;
                }
                colnum++;
                iter++;
            }
            my_throw("Unknown column '" + field_name + "'");
        }
    };

    typedef auto_ptr<mock_pg_result> ap_mock_pg_result;

    class mock_db : public pg_conn_exec {

        virtual ap_pg_result exec(const string & sql) {
            my_throw("Must use parameterized queries! Bad query: " + sql);
        }

        #define COLS(cols...) rs->columns = ARGS_TO_VEC(string, cols)
        #define VALS(vals...) rs->rows.push_back(ARGS_TO_VEC(string, vals))
        #define RETURN_RESULT return ap_pg_result(rs)

        virtual ap_pg_result exec(const string & sql,
                                  const pg_params & params) {
            ap_mock_pg_result rs(new mock_pg_result());
            if (sql == "SELECT intlength_ms, intend_silence_start_ms, "
                       "blndynamically_compressed, intend_quiet_start_ms, "
                       "blnends_with_fade, intbegin_silence_stop_ms, "
                       "intbegin_quiet_stop_ms, blnbegins_with_fade "
                       "FROM tblinstore_media JOIN tblinstore_media_dir "
                       "USING (lnginstore_media_dir) WHERE strdir = ? AND "
                       "strfile = ? AND intlength_ms IS NOT NULL") {

                COLS("intlength_ms", "intend_silence_start_ms",
                     "blndynamically_compressed", "intend_quiet_start_ms",
                     "blnends_with_fade", "intbegin_silence_stop_ms",
                     "intbegin_quiet_stop_ms", "blnbegins_with_fade");
                VALS("60000", "0", "t", "60000", "f", "0", "0", "f");
                RETURN_RESULT;
            }
            else {
                my_throw("Unrecognized SQL: " + sql);
            }
        }

        #undef COLS
        #undef VALS
        #undef RETURN_RESULT
    };

    // Logging function that discards logged messages. Used to suppress
    // logged messages
    void null_logger(const log_info & LI) {}

    // Logging callback function which adds the logging call to the
    // call log
    void log_logger(const log_info & LI) {
        string args = format_log(LI, "%TYPE, '%MESSAGE'");
        calls.push_back(call("log_logger", args));
    }
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
    test_player_get_next_item::patched_mp3_tags mp3tags;
    player_config config;
    player_run_data run_data;
    test_player_get_next_item::mock_db db;

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

        // Add an artist for the test item
        mp3tags.mock_mp3_artists[pe.strmedia] = "<Test Artist>";

        // Add a single programming element to the segment.
        run_data.current_segment->programming_elements.push_back(pe);
        run_data.current_segment->blnloaded = true;
    }

    // Per-test fixture teardown
    void tearDown() {
        mhistory.clear();
        mp3tags.clear();
        run_data.current_segment->reset();
        logging.remove_all_loggers();
    }

    // A basic "smoke test" for get_next_ok_music_item()
    void test_should_work() {
        // Run the tested function
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);

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
                                   db, config, run_data),
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
                                   db, config, run_data),
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
                               db, config, run_data);

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
                               db, config, run_data);

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
                                   db, config, run_data),
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
        string expected_args = "/dir/to/music/mp3s/test.mp3, 45";
        int call_count = 0;
        call_list::iterator it = calls.begin();
        while(it != calls.end()) {
            if (it->func == "song_played_recently") {
                TS_ASSERT_EQUALS(it->args, expected_args);
                ++call_count;
            }
            ++it;
        }
        TS_ASSERT_EQUALS(call_count, 100);
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
                               db, config, run_data);

        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/announcement/mp3s/ann.mp3");
        TS_ASSERT_EQUALS(next_item.cat, SCAT_PROMOS);
    }

    // Should return any item that hasn't played recently.
    void test_should_return_any_non_recent_music_item() {
        using namespace test_player_get_next_item;
        // Add 100 music items to the playlist and history. All of them are
        // in the history except for the 50th one
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 99; ++i) {
            programming_element pe;
            pe.cat = SCAT_PROMOS;
            pe.strmedia = "/dir/to/announcement/mp3s/ann.mp3";
            pe.blnloaded = true;
            pe.cat = SCAT_MUSIC;

            if (i == 49) {
                // The 50th item is not in the music history
                pe.strmedia = "/dir/to/music/mp3s/new.mp3";
            }
            else {
                // All other items are in the (recent) music history
                pe.strmedia = "/dir/to/music/mp3s/old.mp3";
                mhistory.song_played_no_db(pe.strmedia, "<song description>");
            }
            // Add to the playlist:
            pel.push_back(pe);
        }
        run_data.current_segment->set_pel(pel);
        run_data.current_segment->blnrepeat = true;
        run_data.current_segment->blnloaded = true;

        // Add artists for the MP3s
        mp3tags.mock_mp3_artists["/dir/to/music/mp3s/old.mp3"] =
            "<Test Artist>";
        mp3tags.mock_mp3_artists["/dir/to/music/mp3s/new.mp3"] =
            "<Test Artist>";

        // Add a logger callback function, to suppress the debug and warning
        // messages that will be logged (skipping song and file not found)
        logging.add_logger(null_logger);

        // Get the next ok music item:
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);

        // Check the retrieved item
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/new.mp3");
    }

    // Should log a debug message for skipped songs
    void test_should_log_debug_message_for_skipped_songs() {
        using namespace test_player_get_next_item;
        // Add the same MP3 49 times to the playlist and history,
        // followed by an MP3 which isn't in the history
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 49; ++i) {
            programming_element pe;
            pe.cat = SCAT_PROMOS;
            pe.strmedia = "/dir/to/announcement/mp3s/ann.mp3";
            pe.blnloaded = true;
            pe.cat = SCAT_MUSIC;

            if (i == 49) {
                // The 50th item is not in the music history
                pe.strmedia = "/dir/to/music/mp3s/new.mp3";
            }
            else {
                // All other items are in the (recent) music history
                pe.strmedia = "/dir/to/music/mp3s/old.mp3";
                mhistory.song_played_no_db(pe.strmedia, "<song description>");
            }
            // Add to the playlist:
            pel.push_back(pe);
        }
        run_data.current_segment->set_pel(pel);
        run_data.current_segment->blnrepeat = true;
        run_data.current_segment->blnloaded = true;

        // Add artists for the MP3s
        mp3tags.mock_mp3_artists["/dir/to/music/mp3s/old.mp3"] =
            "<Test Artist>";
        mp3tags.mock_mp3_artists["/dir/to/music/mp3s/new.mp3"] =
            "<Test Artist>";

        // Add a logger callback function which keeps track of the logged
        // messages in the call log
        logging.add_logger(log_logger);

        // Get the next ok music item:
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);

        // Check the retrieved item
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/new.mp3");

        // Check the logged messages
        int logged_count = 0;
        string expected_args = "DEBUG, 'Skipping song, it was played "
                               "recently: \"/dir/to/music/mp3s/old.mp3\" - "
                               "\"<MP3 Description>\"'";
        call_list::const_iterator it = calls.begin();
        while (it != calls.end()) {
            if (it->args == expected_args) {
                ++logged_count;
            }
            ++it;
        }
        TS_ASSERT_EQUALS(logged_count, 49);
    }

    // Should log an info message for total number of skipped items.
    void test_should_log_info_message_for_total_skipped_songs() {
        using namespace test_player_get_next_item;
        // Add the same MP3 49 times to the playlist and history,
        // followed by an MP3 which isn't in the history
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 49; ++i) {
            programming_element pe;
            pe.cat = SCAT_PROMOS;
            pe.strmedia = "/dir/to/announcement/mp3s/ann.mp3";
            pe.blnloaded = true;
            pe.cat = SCAT_MUSIC;

            if (i == 49) {
                // The 50th item is not in the music history
                pe.strmedia = "/dir/to/music/mp3s/new.mp3";
            }
            else {
                // All other items are in the (recent) music history
                pe.strmedia = "/dir/to/music/mp3s/old.mp3";
                mhistory.song_played_no_db(pe.strmedia, "<song description>");
            }
            // Add to the playlist:
            pel.push_back(pe);
        }
        run_data.current_segment->set_pel(pel);
        run_data.current_segment->blnrepeat = true;
        run_data.current_segment->blnloaded = true;

        // Add artists for the MP3s
        mp3tags.mock_mp3_artists["/dir/to/music/mp3s/old.mp3"] =
            "<Test Artist>";
        mp3tags.mock_mp3_artists["/dir/to/music/mp3s/new.mp3"] =
            "<Test Artist>";

        // Add a logger callback function which keeps track of the logged
        // messages in the call log
        logging.add_logger(log_logger);

        // Get the next ok music item:
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);

        // Check the retrieved item
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/new.mp3");

        // Look for the expected logged message
        int logged_count = 0;
        string expected_args = "MESSAGE, 'Skipped 49 songs while finding "
                               "songs that haven't played recently. See "
                               "debug log for more info.'";
        call_list::const_iterator it = calls.begin();
        while (it != calls.end()) {
            if (it->args == expected_args) {
                ++logged_count;
            }
            ++it;
        }
        TS_ASSERT_EQUALS(logged_count, 1);
    }

    // Should fail as expected if an ok item couldn't be found (log a warning,
    // clear the music history, and raise an exception)
    void test_should_fail_as_expected_for_no_ok_music_items() {
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

        // Add a logger callback function which keeps track of the logged
        // messages in the call log
        logging.add_logger(log_logger);

        // Test get_next_ok_music_item()
        string expected_error = "I was unable to find a song which has "
                                "not been played recently!";
        TS_ASSERT_THROWS_EQUALS (
            get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                                   db, config, run_data),
            const my_exception &e, e.get_error(), expected_error
        );
        TS_ASSERT_EQUALS(run_data.current_segment->get_num_fetched(), 100);

        // Check for the expected logged warning, and that the music history
        // was closed
        int logged_count = 0;
        bool music_history_cleared = false;
        string expected_args = "WARNING, 'Forced to clear the in-memory (not "
                               "database) music history!'";
        call_list::const_iterator it = calls.begin();

        while (it != calls.end()) {
            if (it->args == expected_args) {
                ++logged_count;
            }
            if (it->func == "music_history::clear") {
                music_history_cleared = true;
            }
            ++it;
        }
        TS_ASSERT_EQUALS(logged_count, 1);
        TS_ASSERT(music_history_cleared);
    }

    // Should not repeat artists too soon, even when the music playlist
    // has a lot of song repetition (which, in older versions of the Player
    // could cause repetition as recent songs are dropped.
    void test_should_alternate_artists_properly_when_heavy_song_repetition() {
        // Create a music playlist and history like this:
        //
        // 0123456789 (Song number)
        // ABCABCABCA (Artists A, B, and C)
        // RR.RR..... (Where R is recently played, and "." is not)
        //
        // Originally we would get this sequence of artists:
        //     CCABCA (songs: 2, 5, 6, 7, 8, 9)
        // This is incorrect because artist C plays twice in succession.
        // Instead, the sequence should be:
        //     CABCA (songs: 2, 6, 7, 8, 9) (song 5 is also skipped)
        using namespace test_player_get_next_item;
        // Put 10 songs into the playlist
        programming_element_list pel;
        run_data.current_segment->reset();
        for (int i = 0; i <= 10; ++i) {
            programming_element pe;
            pe.cat = SCAT_MUSIC;
            pe.strmedia = "/dir/to/music/mp3s/" + itostr(i) + ".mp3";
            pe.blnloaded = true;
            pel.push_back(pe);
            mhistory.song_played_no_db(pe.strmedia,
                                       "<song #" + itostr(i) + ">");
        }
        run_data.current_segment->set_pel(pel);
        run_data.current_segment->blnrepeat = true;
        run_data.current_segment->blnloaded = true;

        // Put songs 0, 1, 3 and 4 in the recently-played songs list
        mhistory.recently_played.insert("/dir/to/music/mp3s/0.mp3");
        mhistory.recently_played.insert("/dir/to/music/mp3s/1.mp3");
        mhistory.recently_played.insert("/dir/to/music/mp3s/3.mp3");
        mhistory.recently_played.insert("/dir/to/music/mp3s/4.mp3");

        // Setup artists for the songs (alternate A, B, and C)
        for (int i=0; i <= 10; i++) {
            string mp3 = "/dir/to/music/mp3s/" + itostr(i) + ".mp3";
            string artist = "<Unknown>";
            switch (i % 3) {
                case 0: artist = "<Artist A>"; break;
                case 1: artist = "<Artist B>"; break;
                case 2: artist = "<Artist C>"; break;
            }
            mp3tags.mock_mp3_artists[mp3] = artist;
        }

        // Add a logger callback function, to suppress the debug and warning
        // messages that will be logged (skipping song and file not found)
        logging.add_logger(null_logger);

        // Get a song (should be 2.mp3)
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/2.mp3");
        // Pretend that song 2 just played (so that it's song artist goes
        // into the song history)
        mhistory.song_played_no_db("/dir/to/music/mp3s/2.mp3", "<desc>");

        // Get the next song (should be 6.mp3)
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/6.mp3");
        // Pretend that the song played:
        mhistory.song_played_no_db("/dir/to/music/mp3s/6.mp3", "<desc>");

        // Get the next song (should be 7.mp3)
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/7.mp3");
        // Pretend that the song played:
        mhistory.song_played_no_db("/dir/to/music/mp3s/7.mp3", "<desc>");

        // Get the next song (should be 8.mp3)
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/8.mp3");
        // Pretend that the song played:
        mhistory.song_played_no_db("/dir/to/music/mp3s/8.mp3", "<desc>");

        // Get the next song (should be 9.mp3)
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               db, config, run_data);
        TS_ASSERT_EQUALS(next_item.strmedia,
                         "/dir/to/music/mp3s/9.mp3");
        // Pretend that the song played:
        mhistory.song_played_no_db("/dir/to/music/mp3s/9.mp3", "<desc>");
    }
};
