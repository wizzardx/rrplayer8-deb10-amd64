/***************************************************************************
                          check_library_versions.h  -  description
                             -------------------
    version              : 0.16
    begin                : Thu Jul 22 2004
    copyright            : (C) 2004 by David Purdy
    email                : david@radioretail.co.za
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// This header file checks the versions of other header files that have been included. Include this
// header last in your project (and in your libraries) to help ensure that your library versions are all the latest.
// - This header will have to be updated with every library version update.

// CHECK HEADER FILE VERSIONS:

// A list of the expected versions:
#define EXPECTED_BUILD_NUM_H_VERSION              5  // Meaning 0.05
#define EXPECTED_CHAR_ARRAY_MATHS_H_VERSION       3  // Meaning 0.03
#define EXPECTED_DIR_LIST_H_VERSION               3  // Meaning 0.03
#define EXPECTED_EVENT_H_VERSION                  5  // Meaning 0.05
#define EXPECTED_LOGGER_H_VERSION                 8  // Meaning 0.08
#define EXPECTED_LOGGING_H_VERSION                8  // Meaning 0.08
#define EXPECTED_MEDIA_PLAYER_H_VERSION           6  // Meaning 0.06
#define EXPECTED_MEDIA_SPLITTER_H_VERSION         8  // Meaning 0.08
#define EXPECTED_MP3_HANDLING_H_VERSION           8  // Meaning 0.08
#define EXPECTED_MP3_TAGS_H_VERSION               6  // Meaning 0.06
#define EXPECTED_RR_PSQL_H_VERSION                9  // Meaning 0.09
#define EXPECTED_RR_SECURITY_H_VERSION            5  // Meaning 0.05
#define EXPECTED_RR_UTILS_H_VERSION               26 // Meaning 0.26
#define EXPECTED_STRING_SPLITTER_H_VERSION        2  // Meaning 0.02
#define EXPECTED_TEMP_DIR_H_VERSION               6  // Meaning 0.06
#define EXPECTED_TIMED_EVENTS_H_VERSION           5  // Meaning 0.05
#define EXPECTED_WINDOWS_MOUNT_H_VERSION          7  // Meaning 0.07
#define EXPECTED_XMMS_CONTROLLER_H_VERSION        6  // Meaning 0.06
#define EXPECTED_XMMS_CONTROLSOCKET_H_VERSION     6  // Meaning 0.06
#define EXPECTED XMMSCTRL_H_VERSION               6  // Meaning 0.06

// build_num.h

#ifdef BUILD_NUM_H
  #ifndef  BUILD_NUM_H_VERSION_CHECKED
    #define BUILD_NUM_H_VERSION_CHECKED
    #ifndef BUILD_NUM_H_VERSION
      #warning BUILD_NUM_H_VERSION is not defined!
    #elif (BUILD_NUM_H_VERSION != EXPECTED_BUILD_NUM_H_VERSION)
      #warning build_num.h is not the correct version!
    #endif
  #endif
#endif

// char_array_maths.h

#ifdef CHAR_ARRAY_MATHS_H
  #ifndef  CHAR_ARRAY_MATHS_H_VERSION_CHECKED
    #define CHAR_ARRAY_MATHS_H_VERSION_CHECKED
    #ifndef CHAR_ARRAY_MATHS_H_VERSION
      #warning CHAR_ARRAY_MATHS_H_VERSION is not defined!
    #elif (CHAR_ARRAY_MATHS_H_VERSION != EXPECTED_CHAR_ARRAY_MATHS_H_VERSION)
      #warning char_array_maths.h is not the correct version!
    #endif
  #endif
#endif

// dir_list.h

#ifdef DIR_LIST_H
  #ifndef  DIR_LIST_H_VERSION_CHECKED
    #define DIR_LIST_H_VERSION_CHECKED
    #ifndef DIR_LIST_H_VERSION
      #warning DIR_LIST_H_VERSION is not defined!
    #elif (DIR_LIST_H_VERSION != EXPECTED_DIR_LIST_H_VERSION)
      #warning dir_list.h is not the correct version!
    #endif
  #endif
#endif

// event.h

#ifdef EVENT_H
  #ifndef  EVENT_H_VERSION_CHECKED
    #define EVENT_H_VERSION_CHECKED
    #ifndef EVENT_H_VERSION
      #warning EVENT_H_VERSION is not defined!

    #elif (EVENT_H_VERSION != EXPECTED_EVENT_H_VERSION)
      #warning event.h is not the correct version!
    #endif
  #endif
#endif

// logger.h

#ifdef LOGGER_H
  #ifndef  LOGGER_H_VERSION_CHECKED
    #define LOGGER_H_VERSION_CHECKED
    #ifndef LOGGER_H_VERSION
      #warning LOGGER_H_VERSION is not defined!
    #elif (LOGGER_H_VERSION != EXPECTED_LOGGER_H_VERSION)
      #warning logger.h is not the correct version!
    #endif
  #endif
#endif

// logging.h

#ifdef LOGGING_H
  #ifndef  LOGGING_H_VERSION_CHECKED
    #define LOGGING_H_VERSION_CHECKED
    #ifndef LOGGING_H_VERSION
      #warning LOGGING_H_VERSION is not defined!
    #elif (LOGGING_H_VERSION != EXPECTED_LOGGING_H_VERSION)
      #warning logging.h is not the correct version!
    #endif
  #endif
#endif

// media_player.h

#ifdef MEDIA_PLAYER_H
  #ifndef  MEDIA_PLAYER_H_VERSION_CHECKED
    #define MEDIA_PLAYER_H_VERSION_CHECKED
    #ifndef MEDIA_PLAYER_H_VERSION
      #warning MEDIA_PLAYER_H_VERSION is not defined!
    #elif (MEDIA_PLAYER_H_VERSION != EXPECTED_MEDIA_PLAYER_H_VERSION)
      #warning media_player.h is not the correct version!
    #endif
  #endif
#endif

// media_splitter.h

#ifdef MEDIA_SPLITTER_H
  #ifndef  MEDIA_SPLITTER_H_VERSION_CHECKED
    #define MEDIA_SPLITTER_H_VERSION_CHECKED
    #ifndef MEDIA_SPLITTER_H_VERSION
      #warning MEDIA_SPLITTER_H_VERSION is not defined!
    #elif (MEDIA_SPLITTER_H_VERSION != EXPECTED_MEDIA_SPLITTER_H_VERSION)
      #warning media_splitter.h is not the correct version!
    #endif
  #endif
#endif

// mp3_handling.h

#ifdef MP3_HANDLING_H
  #ifndef  MP3_HANDLING_H_VERSION_CHECKED
    #define MP3_HANDLING_H_VERSION_CHECKED
    #ifndef MP3_HANDLING_H_VERSION
      #warning MP3_HANDLING_H_VERSION is not defined!
    #elif (MP3_HANDLING_H_VERSION != EXPECTED_MP3_HANDLING_H_VERSION)
      #warning mp3_handling.h is not the correct version!
    #endif
  #endif
#endif

// mp3_tags.h

#ifdef MP3_TAGS_H
  #ifndef  MP3_TAGS_H_VERSION_CHECKED
    #define MP3_TAGS_H_VERSION_CHECKED
    #ifndef MP3_TAGS_H_VERSION
      #warning MP3_TAGS_H_VERSION is not defined!
    #elif (MP3_TAGS_H_VERSION != EXPECTED_MP3_TAGS_H_VERSION)
      #warning mp3_tags.h is not the correct version!
    #endif
  #endif
#endif

// rr_psql.h

#ifdef RR_PSQL_H
  #ifndef RR_PSQL_H_VERSION_CHECKED
    #define RR_PSQL_H_VERSION_CHECKED
    #ifndef RR_PSQL_H_VERSION
      #warning RR_PSQL_H_VERSION is not defined!
    #elif (RR_PSQL_H_VERSION != EXPECTED_RR_PSQL_H_VERSION)
      #warning rr_psql.h is not the correct version!
    #endif
  #endif
#endif

// rr_security.h

#ifdef RR_SECURITY_H
  #ifndef  RR_SECURITY_H_VERSION_CHECKED
    #define RR_SECURITY_H_VERSION_CHECKED
    #ifndef RR_SECURITY_H_VERSION
      #warning RR_SECURITY_H_VERSION is not defined!
    #elif (RR_SECURITY_H_VERSION != EXPECTED_RR_SECURITY_H_VERSION)
      #warning rr_security.h is not the correct version!
    #endif
  #endif
#endif

// rr_utils.h

#ifdef RR_UTILS_H
  #ifndef  RR_UTILS_H_VERSION_CHECKED
    #define RR_UTILS_H_VERSION_CHECKED
    #ifndef RR_UTILS_H_VERSION
      #warning RR_UTILS_H_VERSION is not defined!
    #elif (RR_UTILS_H_VERSION != EXPECTED_RR_UTILS_H_VERSION)
      #warning rr_utils.h is not the correct version!
    #endif
  #endif
#endif

// string_splitter.h

#ifdef STRING_SPLITTER_H
  #ifndef  STRING_SPLITTER_VERSION_CHECKED
    #define STRING_SPLITTER_VERSION_CHECKED
    #ifndef STRING_SPLITTER_H_VERSION
      #warning STRING_SPLITTER_H_VERSION is not defined!
    #elif (STRING_SPLITTER_H_VERSION != EXPECTED_STRING_SPLITTER_H_VERSION)
      #warning string_splitter.h is not the correct version!
    #endif
  #endif
#endif

// temp_dir.h

#ifdef TEMP_DIR_H
  #ifndef  TEMP_DIR_H_VERSION_CHECKED
    #define TEMP_DIR_H_VERSION_CHECKED
    #ifndef TEMP_DIR_H_VERSION
      #warning TEMP_DIR_H_VERSION is not defined!
    #elif (TEMP_DIR_H_VERSION != EXPECTED_TEMP_DIR_H_VERSION)
      #warning temp_dir.h is not the correct version!
    #endif
  #endif
#endif

// timed_events.h

#ifdef TIMED_EVENTS_H
  #ifndef  TIMED_EVENTS_H_VERSION_CHECKED
    #define TIMED_EVENTS_H_VERSION_CHECKED
    #ifndef TIMED_EVENTS_H_VERSION
      #warning TIMED_EVENTS_H_VERSION is not defined!
    #elif (TIMED_EVENTS_H_VERSION != EXPECTED_TIMED_EVENTS_H_VERSION)
      #warning timed_events.h is not the correct version!
    #endif
  #endif
#endif

// windows_mount.h

#ifdef WINDOWS_MOUNT_H
  #ifndef  WINDOWS_MOUNT_H_VERSION_CHECKED
    #define WINDOWS_MOUNT_H_VERSION_CHECKED
    #ifndef WINDOWS_MOUNT_H_VERSION
      #warning WINDOWS_MOUNT_H_VERSION is not defined!
    #elif (WINDOWS_MOUNT_H_VERSION != EXPECTED_WINDOWS_MOUNT_H_VERSION)
      #warning windows_mount.h is not the correct version!
    #endif
  #endif
#endif

// xmms_controller.h

#ifdef XMMS_CONTROLLER_H
  #ifndef  XMMS_CONTROLLER_H_VERSION_CHECKED
    #define XMMS_CONTROLLER_H_VERSION_CHECKED
    #ifndef XMMS_CONTROLLER_H_VERSION
      #warning XMMS_CONTROLLER_H_VERSION is not defined!
    #elif (XMMS_CONTROLLER_H_VERSION != EXPECTED_XMMS_CONTROLLER_H_VERSION)
      #warning xmms_controller.h is not the correct version!
    #endif
  #endif
#endif

// xmms_controlsocket.h

#ifdef XMMS_CONTROLSOCKET_H
  #ifndef  XMMS_CONTROLSOCKET_H_VERSION_CHECKED
    #define XMMS_CONTROLSOCKET_H_VERSION_CHECKED
    #ifndef XMMS_CONTROLSOCKET_H_VERSION
      #warning XMMS_CONTROLSOCKET_H_VERSION is not defined!
    #elif (XMMS_CONTROLSOCKET_H_VERSION != EXPECTED_XMMS_CONTROLSOCKET_H_VERSION)
      #warning xmms_controlsocket.h is not the correct version!
    #endif
  #endif
#endif

// xmmsctrl.h

#ifdef XMMSCTRL_H
  #ifndef  XMMSCTRL_H_VERSION_CHECKED
    #define XMMSCTRL_H_VERSION_CHECKED
    #ifndef XMMSCTRL_H_VERSION
      #warning XMMSCTRL_H_VERSION is not defined!
    #elif (XMMSCTRL_H_VERSION != EXPECTED_XMMSCTRL_H_VERSION)
      #warning xmmsctrl.h is not the correct version!
    #endif
  #endif
#endif
