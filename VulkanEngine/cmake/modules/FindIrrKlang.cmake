set( _irrklang_HEADER_SEARCH_DIRS
        "/usr/include"
        "/usr/local/include"
        "${CMAKE_SOURCE_DIR}/includes"
        "C:/Program Files (x86)/irrklang/include" )
set( _irrklang_LIB_SEARCH_DIRS
        "/usr/lib"
        "/usr/local/lib"
        "${CMAKE_SOURCE_DIR}/lib"
        "C:/Program Files (x86)/irrklang/" )

# Check environment for root search directory
set( _irrklang_ENV_ROOT $ENV{IRRKLANG_ROOT} )
if( NOT IRRKLANG_ROOT AND _irrklang_ENV_ROOT )
    set(IRRKLANG_ROOT ${_irrklang_ENV_ROOT} )
endif()

# Put user specified location at beginning of search
if( IRRKLANG_ROOT )
    list( INSERT _irrklang_HEADER_SEARCH_DIRS 0 "${IRRKLANG_ROOT}/include" )
    if(WIN32)
        list( INSERT _irrklang_LIB_SEARCH_DIRS 0 "${IRRKLANG_ROOT}/lib" )
    else()
        list( INSERT _irrklang_LIB_SEARCH_DIRS 0 "${IRRKLANG_ROOT}/bin" )
    endif()
endif()

# Search for the header
FIND_PATH(IRRKLANG_INCLUDE_DIR "irrKlang.h"
        PATHS ${_irrklang_HEADER_SEARCH_DIRS} )

# Search for the library
SET(_irrklang_LIBRARY_NAME "irrKlang")
if(UNIX AND NOT APPLE)
    SET(_irrklang_LIBRARY_NAME "irrKlang.so")
elseif(UNIX AND APPLE)
    SET(_irrklang_LIBRARY_NAME "libirrklang.dylib")
endif(UNIX AND NOT APPLE)

FIND_LIBRARY(IRRKLANG_LIBRARY NAMES ${_irrklang_LIBRARY_NAME}
        PATHS ${_irrklang_LIB_SEARCH_DIRS} )
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(IrrKlang DEFAULT_MSG
        IRRKLANG_LIBRARY IRRKLANG_INCLUDE_DIR)
