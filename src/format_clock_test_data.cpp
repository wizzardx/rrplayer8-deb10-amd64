/***************************************************************************
                          format_clock_test_data.cpp  -  description
                             -------------------
    begin                : Thu Mar 17 2005
    copyright            : (C) 2005 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#include "format_clock_test_data.h"
#include "common/my_string.h"
#include "common/file.h"
#include "common/dir_list.h"
#include "common/logging.h"

void format_clock_test_data::clear_tables() {
  log_message("Deleting data from Format Clock tables...");
  db.exec("DELETE FROM tblfc_seg");
  db.exec("DELETE FROM tblfc_media");
  db.exec("DELETE FROM tlkfc_sub_cat");
  db.exec("DELETE FROM tblfc_sched_day");
  db.exec("DELETE FROM tblfc_sched");
  db.exec("DELETE FROM tblfc");
}

void format_clock_test_data::generate_test_data() {
  log_message("Generating Test Format Clock Data....");

  // tblfc
  db.exec("INSERT INTO tblfc VALUES (1, 'Default');");
  db.exec("INSERT INTO tblfc VALUES (2, 'Morning');");
  db.exec("INSERT INTO tblfc VALUES (3, 'Afternoon');");
  db.exec("INSERT INTO tblfc VALUES (4, 'Evening');");
  db.exec("INSERT INTO tblfc VALUES (5, 'Night');");

  // tbldefs
  db.exec("DELETE FROM tbldefs WHERE strdef='lngDefaultFormatClock';");
  db.exec("INSERT INTO tbldefs (strdef, strdatatype, strdef_val) VALUES ('lngDefaultFormatClock', 'lng', '1');");

  // tblfc_sched;
  db.exec("INSERT INTO tblfc_sched VALUES (1, NULL, NULL, NULL)");

  // tblfc_sched_day;
  db.exec("INSERT INTO tblfc_sched_day VALUES (1, 1, 2, time '07:00:00', time '11:59:59')");
  db.exec("INSERT INTO tblfc_sched_day VALUES (2, 1, 3, time '12:00:00', time '16:59:59')");
  db.exec("INSERT INTO tblfc_sched_day VALUES (3, 1, 4, time '17:00:00', time '18:59:59')");
  db.exec("INSERT INTO tblfc_sched_day VALUES (4, 1, 5, time '19:00:00', time '23:59:59')");

  // tblfc_sub_cat
  {
    int intsub_cat_count = 0;
    pg_result rs = db.exec("SELECT lngfc_cat, strdir FROM tlkfc_cat");
    while (rs) {
      long lngfc_cat = strtoi(rs.field("lngfc_cat"));
      string strdir = rs.field("strdir", "");

      if (dir_exists(strdir)) {
        // We have a category sub-directory. List the sub-directories
        dir_list cat_dir(strdir, "", DT_DIR);
        while (cat_dir) {
          string strsub_cat_dir=cat_dir;
          // Add a sub-category...
          ++intsub_cat_count;
          db.exec("INSERT INTO tlkfc_sub_cat (lngfc_sub_cat, lngfc_cat, strname, strdir) VALUES (" + itostr(intsub_cat_count) + ", " + itostr(lngfc_cat) + ", '" + strsub_cat_dir + "', '" + strdir + strsub_cat_dir +  "/')");

          // Go into each sub-directory, list the media and add to tblfc_media
          dir_list sub_cat_dir(strdir + strsub_cat_dir +  "/");

          while (sub_cat_dir) {
            string strfile = sub_cat_dir;
            db.exec("INSERT INTO tblfc_media (strfile, lngcat, lngsub_cat, dtmrelevant_from, dtmrelevant_until) VALUES ('" + strfile + "', " + ltostr(lngfc_cat) + ", " + itostr(intsub_cat_count) + ", '2005-01-01', '2005-12-31')");
          }
        }
      }
      rs ++;
    }
  }

  // tblfc_seg
  {
    // Populate all the format clock segments.
    // Get a recorset ready for general abuse (ignore music bed category and regular music and silence.. regular music is hacked in later...)
    pg_result rs = db.exec("SELECT lngfc_sub_cat, lngfc_cat FROM tlkfc_sub_cat WHERE lngfc_cat != 8 AND lngfc_cat != 2 AND lngfc_cat != 9");
    for (long lngfc=1; lngfc < 6; ++lngfc) {
      // Break the hour up into 5-minute segments, of random category & sub-category:
      int intfrom_min=0;
      int intto_min=4;

      while (intfrom_min < 60) {
        string strfrom = itostr(intfrom_min) + ":00";
        string strto   = itostr(intto_min) + ":59";
        if (intfrom_min < 10) {
          strfrom = "0" + strfrom;
          strto   = "0" + strto;
        }
        strfrom = "00:" + strfrom;
        strto   = "00:" + strto;

        // Get a random category & sub-category:
        long lngcat = 0;
        string lngsub_cat = "0";
        {
          int intrand = rand() % rs.size();
          rs.movefirst();
          for (int i = 0; i < intrand; i++) {
            rs++;
          }
          lngcat = strtol(rs.field("lngfc_cat"));
          lngsub_cat = rs.field("lngfc_sub_cat");
        }

        // Also a 1 in 3 chance that we will override the category & use music...
        if(rand()%3 == 0) {
          lngcat=2;
          lngsub_cat = "'/home/radman/david_stuff/music/ff8music/'";
        }

        // Also get a random alternate category & sub-category:
        long lngalt_cat = 0;
        string lngalt_sub_cat = "0";
        {
          int intrand= rand() % rs.size();
          rs.movefirst();
          for (int i = 0; i < intrand; i++) {
            rs++;
          }
          lngalt_cat = strtol(rs.field("lngfc_cat"));
          lngalt_sub_cat = rs.field("lngfc_sub_cat");
        }

        // Also a 1 in 3 chance that we will override the alternate category & use music...
        if(rand()%3 == 0) {
          lngalt_cat=2;
          lngalt_sub_cat = "'/home/radman/david_stuff/music/ff7music/'";
        }

        // Choose a sequence (1-3)
        string lngspecific_seq_media = "NULL";

        long lngseq=rand()%3 + 1;
        if (lngseq==3 && lngcat != 2) { // Only choose a specific file if this is not a music segment...
          // Specific

          // Choose a random media item for "specific" playback:
          pg_result rsmedia = db.exec("SELECT lngfc_media FROM tblfc_media WHERE lngcat=" + ltostr(lngcat) + " AND lngsub_cat=" + lngsub_cat);

          int intrand = rand() % rsmedia.size();
          rsmedia.movefirst();

          for (int i = 0; i < intrand; i++) rsmedia++;
          lngspecific_seq_media = rsmedia.field("lngfc_media");
        }

        // Allow promos if this is a music segment...
        bool ysnpromos = (lngcat == 2);

        // Has a music bed? .. yes, if it is not a music segment
        bool ysnunderlying_music = false;
        string lngunderlying_music_sub_cat = "NULL";
        if (lngcat != 2) {
          pg_result rs = db.exec("SELECT lngfc_sub_cat FROM tlkfc_sub_cat WHERE lngfc_cat = 8");
          int intrand = rand() % rs.size();
          for (int i=0; i<intrand; i++) rs++;
          lngunderlying_music_sub_cat = rs.field("lngfc_sub_cat");
          ysnunderlying_music = true;
        }

        // Crossfade in this segment? (half of the time yes, half of the time no)
        bool ysncrossfade=(rand() % 2);

        int intmax_age=1000;
        bool ysnpremature = false; // No premature ads...
        bool ysnrepeat    = (rand() % 2); // Repeat media in this segment?

        // Now we have all the info we need to add a segment.
        db.exec("INSERT INTO tblfc_seg (lngfc, lngcat, strsub_cat, lngseq, lngspecific_seq_media, dtmstart, dtmend, ysnpromos, lngalt_cat, stralt_sub_cat, ysnmusic_bed, lngmusic_bed_sub_cat, ysncrossfade, intmax_age, ysnpremature, ysnrepeat) "
                               "VALUES (" + itostr(lngfc) + ", " +
                               itostr(lngcat) + ", " +
                               lngsub_cat + ", " +
                               itostr(lngseq) + ", " +
                               lngspecific_seq_media + ", '" +
                               strfrom + "', '" +
                               strto + "', " +
                               (ysnpromos ? "'1'":"'0'") + ", " +
                               itostr(lngalt_cat) + ", " +
                               lngalt_sub_cat + ", " +
                               (ysnunderlying_music?"'1'":"'0'") + ", " +
                               lngunderlying_music_sub_cat + ", " +
                               (ysncrossfade?"'1'":"'0'") + ", " +
                               itostr(intmax_age) + ", " +
                               (ysnpremature?"'1'":"'0'") + ", " +
                               (ysnrepeat?"'1'":"'0'") + ");");

        // Go to the next 5 minutes:
        intfrom_min += 5;
        intto_min += 5;
      }
    }
  }
}
