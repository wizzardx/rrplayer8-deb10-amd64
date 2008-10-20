
// test_player_get_next_item.h

// Tests for the logic found in src/player_get_next_item.cpp

#include <cxxtest/TestSuite.h>

#include "player.h"

// Tests for get_next_ok_music_item()

class TestGetNextOkMusicItem : public CxxTest::TestSuite
{
public:
    void testAddition( void )
    {
        TS_ASSERT( 1 + 1 > 1 );
        TS_ASSERT_EQUALS( 1 + 1, 2 );
    }
};


