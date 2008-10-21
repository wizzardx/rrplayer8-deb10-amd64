
// test_player_get_next_item.h

// Tests for the logic found in src/player_get_next_item.cpp

#include <cxxtest/TestSuite.h>

#include "player.h"
#include "common/my_string.h"

// Tests for get_next_ok_music_item()

class TestGetNextOkMusicItem : public CxxTest::TestSuite
{
public:

    music_history mhistory;
    mp3_tags mp3tags;
    pg_connection db;
    player_config config;
    player_run_data run_data;
    programming_element next_item;

    // Constructor
    TestGetNextOkMusicItem() {
        // Connect to database
        db.open("dbname=schedule_test");
    }

    // Per-test fixture setup
    void setUp()
    {
        // Setup objects
        programming_element pe;
        pe.cat = SCAT_MUSIC;
        pe.strmedia = "/dir/to/music/mp3s/test.mp3";
        pe.blnloaded = true;
        run_data.current_segment.programming_elements.clear();
        run_data.current_segment.programming_elements.push_back(pe);
        run_data.current_segment.blnloaded = true;
    }

    // A basic "smoke test" for get_next_ok_music_item()
    void test_should_work()
    {
        // Make a DB transaction and put some test data in the database
        pg_transaction trans(db);
        _setup_test_db_data(trans);

        // Run the tested function
        int intstarts_ms = 5000;
        get_next_ok_music_item(next_item, intstarts_ms, mhistory, mp3tags,
                               trans, config, run_data);

        // Check the output of the tested function
        TS_ASSERT(next_item.blnloaded);
        TS_ASSERT_EQUALS(next_item.cat, SCAT_MUSIC);
        TS_ASSERT_EQUALS(next_item.strmedia, "/dir/to/music/mp3s/test.mp3");

        // Rollback the DB transaction
        trans.abort();
    }

    // Helper methods
    void _setup_test_db_data(pg_conn_exec & db)
    {
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
};

