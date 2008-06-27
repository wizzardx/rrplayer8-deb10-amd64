/***************************************************************************
                          format_clock_test_data.h  -  description
                             -------------------
    begin                : Thu Mar 17 2005
    copyright            : (C) 2005 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

#ifndef FORMAT_CLOCK_TEST_DATA_H
#define FORMAT_CLOCK_TEST_DATA_H

#include "common/my_time.h"
#include <cstdlib>

// Forward declarations:
class pg_connection;

// Class that exists only to create Format Clock test data.
class format_clock_test_data {
public:
  format_clock_test_data(pg_connection & DB) : db(DB) { srand (time (0)); };
  ~format_clock_test_data() {};

  void clear_tables();
  void generate_test_data();

private:
  pg_connection & db;
};

#endif
