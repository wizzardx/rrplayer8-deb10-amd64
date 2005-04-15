####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = player

## INCLUDES were found outside kdevelop specific part

player_SOURCES = format_clock_test_data.cpp segment.cpp programming_element.cpp player.cpp main.cpp 
player_LDADD   = ./common/libcommon.a -lglib -lpqxx

SUBDIRS = common 

EXTRA_DIST = main.cpp player.cpp player.h programming_element.cpp programming_element.h segment.cpp segment.h format_clock_test_data.cpp format_clock_test_data.h categories.h 

####### kdevelop will overwrite this part!!! (end)############
# set the include path found by configure
INCLUDES= $(all_includes)
 
# the library search path.
player_LDFLAGS = $(all_libraries) 
                                                                                
