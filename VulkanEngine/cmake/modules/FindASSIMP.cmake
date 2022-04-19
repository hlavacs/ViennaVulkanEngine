if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ASSIMP_ARCHITECTURE "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ASSIMP_ARCHITECTURE "x86")
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

if(WIN32)
    set(ASSIMP_ROOT_DIR CACHE PATH "ASSIMP root directory")

    # Find path of each library
    find_path(ASSIMP_INCLUDE_DIRS
            NAMES
            assimp/anim.h
            HINTS
            ${ASSIMP_ROOT_DIR}/include
            )

    if(MSVC12)
        set(ASSIMP_MSVC_VERSION "vc120")
    elseif(MSVC14)
        set(ASSIMP_MSVC_VERSION "vc140")
    elseif(MSVC17)
        set(ASSIMP_MSVC_VERSION "vc140")
    endif(MSVC12)

    if(MSVC12 OR MSVC14)

        find_path(ASSIMP_LIBRARY_DIR
                NAMES
                assimp-${ASSIMP_MSVC_VERSION}-mt.lib
                HINTS
                ${ASSIMP_ROOT_DIR}/lib/${ASSIMP_ARCHITECTURE}
                )

        find_library(ASSIMP_LIBRARY_RELEASE				assimp-${ASSIMP_MSVC_VERSION}-mt.lib 			PATHS ${ASSIMP_LIBRARY_DIR}/Release)
        find_library(ASSIMP_LIBRARY_DEBUG				assimp-${ASSIMP_MSVC_VERSION}-mt.lib			PATHS ${ASSIMP_LIBRARY_DIR}/Debug)

        set(ASSIMP_LIBRARY
                optimized 	${ASSIMP_LIBRARY_RELEASE}
                debug		${ASSIMP_LIBRARY_DEBUG}
                )

        set(ASSIMP_LIBRARIES "ASSIMP_LIBRARY_RELEASE" "ASSIMP_LIBRARY_DEBUG")

        FUNCTION(ASSIMP_COPY_BINARIES TargetDirectory)
            ADD_CUSTOM_TARGET(AssimpCopyBinaries
                    COMMAND ${CMAKE_COMMAND} -E copy ${ASSIMP_ROOT_DIR}/lib/${ASSIMP_ARCHITECTURE}/Debug/assimp-${ASSIMP_MSVC_VERSION}-mt.dll 	${TargetDirectory}/Debug/assimp-${ASSIMP_MSVC_VERSION}-mt.dll
                    COMMAND ${CMAKE_COMMAND} -E copy ${ASSIMP_ROOT_DIR}/lib/${ASSIMP_ARCHITECTURE}/Release/assimp-${ASSIMP_MSVC_VERSION}-mt.dll 		${TargetDirectory}/Release/assimp-${ASSIMP_MSVC_VERSION}-mt.dll
                    COMMENT "Copying Assimp binaries to '${TargetDirectory}'"
                    VERBATIM)
        ENDFUNCTION(ASSIMP_COPY_BINARIES)

    endif()

else(WIN32)

    find_path(
            ASSIMP_INCLUDE_DIRS
            NAMES assimp/postprocess.h assimp/scene.h assimp/version.h assimp/config.h assimp/cimport.h
            PATHS /usr/local/include
            PATHS /usr/include/

    )

    find_library(
            ASSIMP_LIBRARY
            NAMES assimp
            PATHS /usr/local/lib/
            PATHS /usr/lib64/
            PATHS /usr/lib/
    )

    if (ASSIMP_INCLUDE_DIRS AND ASSIMP_LIBRARY)
        SET(ASSIMP_FOUND TRUE)
    ENDIF (ASSIMP_INCLUDE_DIRS AND ASSIMP_LIBRARY)

    if (ASSIMP_FOUND)
        if (NOT ASSIMP_FIND_QUIETLY)
            message(STATUS "Found asset importer library: ${ASSIMP_LIBRARY}")
        endif (NOT ASSIMP_FIND_QUIETLY)
    else (ASSIMP_FOUND)
        if (ASSIMP_FIND_REQUIRED)
            message(FATAL_ERROR "Could not find asset importer library")
        endif (ASSIMP_FIND_REQUIRED)
    endif (ASSIMP_FOUND)

endif(WIN32)
