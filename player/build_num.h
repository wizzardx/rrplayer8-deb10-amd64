/***************************************************************************
                          build_num.h  -  Provides a function you can use to retrieve the build number
                             -------------------
    version              : v0.05
    begin                : Fri Aug 29 2003
    copyright            : (C) 2003 by David Purdy
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

#ifndef BUILD_NUM_H
#define BUILD_NUM_H
#define BUILD_NUM_H_VERSION 5 // Meaning 0.05

#include "check_library_versions.h" // Always last: Check the versions of included libraries.

/**
  *@author David Purdy
  */

int GetBuildNum(); // Call this function to retrieve the project's Build Number.
                              // - The build number is controlled by build_num.txt, and
                              //   is updated by the make process. If BUILD_NUM is
                              //   not defined, this function will update the Makefile and ask
                              //   you to recompile. The function will return -1 if there
                              //   are any problems retrieving a build number.
  
#endif
