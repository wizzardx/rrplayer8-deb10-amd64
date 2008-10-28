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

}

// Tests for the segment class

class TestSegment : public CxxTest::TestSuite {
public:
    // Members
    test_segment::patched_mp3_tags mp3tags;
    music_history mhistory;
    player_config config;
    test_segment::mock_db db;

    // Per-test fixture setup
    void setUp() {
        mp3tags.clear();
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
            seg.get_next_item(pe, db, 5000, config, mp3tags, mhistory, false);
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
            seg.get_next_item(pe, db, 5000, config, mp3tags, mhistory, false);
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
            seg.get_next_item(pe, db, 5000, config, mp3tags, mhistory, false);
        }

        // Get the number of unique artists in the remaining playlist:
        // (Should be 10 total - the first 30 are included because the
        // playlist is setup to repeat)
        TS_ASSERT_EQUALS(seg.count_remaining_playlist_artists(mp3tags), 10);
    }
};
