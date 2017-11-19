# - Locate libmodplug library
# This module defines
#  MODPLUG_LIBRARY, the library to link against
#  MODPLUG_FOUND, if false, do not try to link to libmad
#  MODPLUG_INCLUDE_DIR, where to find headers.

IF(MODPLUG_LIBRARY AND MODPLUG_INCLUDE_DIR)
  # in cache already
  SET(MODPLUG_FIND_QUIETLY TRUE)
ENDIF(MODPLUG_LIBRARY AND MODPLUG_INCLUDE_DIR)

SET(MODPLUG_SEARCH_PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local
	/usr
	/sw # Fink
	/opt/local # DarwinPorts
	/opt/csw # Blastwave
	/opt
	${MODPLUG_PATH}
)

FIND_PATH(MODPLUG_INCLUDE_DIR
  modplug.h
  PATHS
  $ENV{MODPLUG_DIR}/include
  /usr/local/include
  /usr/include
  /sw/include
  /opt/local/include
  /opt/csw/include
  /opt/include
  PATHS ${MODPLUG_SEARCH_PATHS}
)

FIND_LIBRARY(MODPLUG_LIBRARY
  NAMES modplug libmodplug
  PATHS
  $ENV{MODPLUG_DIR}/lib
  /usr/local/lib
  /usr/lib
  /usr/local/X11R6/lib
  /usr/X11R6/lib
  /sw/lib
  /opt/local/lib
  /opt/csw/lib
  /opt/lib
  /usr/freeware/lib64
  PATHS ${MODPLUG_SEARCH_PATHS}
)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(MODPLUG REQUIRED_VARS MODPLUG_LIBRARY MODPLUG_INCLUDE_DIR)