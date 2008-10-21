// test_segment.h

// Tests for the logic found in src/segment.cpp

#include <cxxtest/TestSuite.h>

#include "segment.h"
#include "common/my_string.h"

// Tests for the segment class

class TestSegment : public CxxTest::TestSuite
{
public:
    // Helper type definitions
    typedef auto_ptr<pg_transaction> ap_pg_transaction;

    // Members
    mp3_tags mp3tags;
    music_history mhistory;
    player_config config;
    ap_pg_transaction trans;

    // Per-test fixture setup
    void setUp()
    {
        pg_connection db;
        db.open("dbname=schedule_test");
        trans = ap_pg_transaction(new pg_transaction(db));
        _setup_test_db_data(*trans);
    }

    // Per-test fixture teardown
    void tearDown()
    {
        trans->abort();
    }

    // segment::set_pel() should work correctly
    void test_set_pel_should_work()
    {
        // Declare variables
        programming_element_list pel;
        programming_element pe;
        segment seg;
        seg.blnloaded = true;

        // Update programming element list
        pe.cat = SCAT_MUSIC;
        pe.blnloaded = true;
        for (int i = 0; i <= 99; i++)
        {
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
        for (int i = 0; i <= 99; i++)
        {
            seg.get_next_item(pe, *trans, 5000, config, mp3tags, mhistory,
                              false);
            string expected_media = "/dir/to/music/mp3s/song" + itostr(i) +
                                    ".mp3";
            TS_ASSERT_EQUALS(pe.strmedia, expected_media);
        }
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

        // Insert records for the songs
        sql = "INSERT INTO tblinstore_media (lnginstore_media_dir, strfile, "
                                            "intlength_ms, "
                                            "blndynamically_compressed) "
              "VALUES (?, ?, ?, ?)";
        for (int i = 0; i <= 99; i++)
        {
            string media = "song" + itostr(i) + ".mp3";
            params = ARGS_TO_PG_PARAMS(ltostr(lnginstore_media_dir),
                                    psql_str(media),
                                    "60000", psql_bool(true));
            db.exec(sql, params);
        }
    }
};
