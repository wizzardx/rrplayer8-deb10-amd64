INCLUDES = 
METASOURCES = AUTO
noinst_PROGRAMS = tests
tests_SOURCES = categories.h music_history.cpp music_history.h player.cpp \
	player.h player_config.h player_constants.h player_get_next_item.cpp \
	player_maintenance.cpp player_playback_transition.cpp player_run_data.cpp player_run_data.h \
	player_types.h player_util.cpp player_util.h programming_element.cpp \
	programming_element.h segment.cpp segment.h tests.cpp

TESTS = test_cxxtest.h test_player_get_next_item.h test_segment.h
tests.cpp: $(TESTS)
	cxxtestgen.pl -o $@ --error-printer $^

tests_LDADD = $(top_builddir)/src/common/libcommon.a -lxmms -lpqxx
noinst_HEADERS = test_player_get_next_item.h
