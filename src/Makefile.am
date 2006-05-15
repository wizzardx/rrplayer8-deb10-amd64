bin_PROGRAMS = player
player_SOURCES = player.cpp main.cpp format_clock_test_data.cpp \
			programming_element.cpp segment.cpp player_run_data.cpp player_get_next_item.cpp \
		player_playback_transition.cpp player_maintenance.cpp player_util.cpp

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
player_LDFLAGS = $(all_libraries) 

SUBDIRS = common
player_LDADD = $(top_builddir)/src/common/libcommon.a /usr/lib/libglib.so \
	/usr/lib/libpqxx.so
noinst_HEADERS = player_run_data.h player_constants.h player_config.h \
	player_util.h
