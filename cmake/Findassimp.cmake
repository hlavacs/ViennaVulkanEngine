	
if(WIN32)
	unset(ASSIMP_ROOT_DIR CACHE	)
	find_path(ASSIMP_ROOT_DIR 
		NAMES assimp/CMakeLists.txt
		HINTS ${PROJECT_SOURCE_DIR}/..
		NO_CACHE
	)
	set(ASSIMP_ROOT_DIR ${ASSIMP_ROOT_DIR}assimp)

	unset(ASSIMP_INCLUDE_DIR CACHE)
	find_path(ASSIMP_INCLUDE_DIR
		NAMES assimp/anim.h
		HINTS ${ASSIMP_ROOT_DIR}/include
		NO_CACHE
	)

	find_path(ASSIMP_LIBRARY_DIR
		NAMES Release/assimp-vc${MSVC_TOOLSET_VERSION}-mt.lib
		HINTS ${ASSIMP_ROOT_DIR}/lib
		NO_CACHE
	)
		
	find_library(ASSIMP_LIBRARY_RELEASE	
		NAMES Release/assimp-vc${MSVC_TOOLSET_VERSION}-mt.lib 
		HINTS ${ASSIMP_ROOT_DIR}/lib
		NO_CACHE
	)

	find_library(ASSIMP_LIBRARY_DEBUG
		NAMES Debug/assimp-vc${MSVC_TOOLSET_VERSION}-mtd.lib
		HINTS ${ASSIMP_ROOT_DIR}/lib
		NO_CACHE
	)
		
	set(ASSIMP_LIBRARY 
		optimized 	${ASSIMP_LIBRARY_RELEASE}
		debug		${ASSIMP_LIBRARY_DEBUG}
	)
		
	set(ASSIMP_LIBRARIES "ASSIMP_LIBRARY_RELEASE" "ASSIMP_LIBRARY_DEBUG")

	FUNCTION(ASSIMP_COPY_BINARIES TargetDirectory)
		ADD_CUSTOM_TARGET(AssimpCopyBinaries
			COMMAND ${CMAKE_COMMAND} -E copy ${ASSIMP_ROOT_DIR}/bin${ASSIMP_ARCHITECTURE}/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll 	${TargetDirectory}/Debug/assimp-${ASSIMP_MSVC_VERSION}-mtd.dll
			COMMAND ${CMAKE_COMMAND} -E copy ${ASSIMP_ROOT_DIR}/bin${ASSIMP_ARCHITECTURE}/assimp-${ASSIMP_MSVC_VERSION}-mt.dll 		${TargetDirectory}/Release/assimp-${ASSIMP_MSVC_VERSION}-mt.dll
		COMMENT "Copying Assimp binaries to '${TargetDirectory}'"
		VERBATIM)
	ENDFUNCTION(ASSIMP_COPY_BINARIES)

	if (ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARIES)
		SET(assimp_FOUND TRUE)
		#message("${ASSIMP_INCLUDE_DIR} ${ASSIMP_LIBRARY_DIR} ${ASSIMP_LIBRARY_RELEASE} ${ASSIMP_LIBRARY_DEBUG}")
	ENDIF (ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARIES)
	
else(WIN32)

	find_path(
	  assimp_INCLUDE_DIRS
	  NAMES assimp/postprocess.h assimp/scene.h assimp/version.h assimp/config.h assimp/cimport.h
	  PATHS /usr/local/include
	  PATHS /usr/include/

	)

	find_library(
	  assimp_LIBRARIES
	  NAMES assimp
	  PATHS /usr/local/lib/
	  PATHS /usr/lib64/
	  PATHS /usr/lib/
	)

	if (assimp_INCLUDE_DIRS AND assimp_LIBRARIES)
	  SET(assimp_FOUND TRUE)
	ENDIF (assimp_INCLUDE_DIRS AND assimp_LIBRARIES)

	if (assimp_FOUND)
	  if (NOT assimp_FIND_QUIETLY)
		message(STATUS "Found asset importer library: ${assimp_LIBRARIES}")
	  endif (NOT assimp_FIND_QUIETLY)
	else (assimp_FOUND)
	  if (assimp_FIND_REQUIRED)
		message(FATAL_ERROR "Could not find asset importer library")
	  endif (assimp_FIND_REQUIRED)
	endif (assimp_FOUND)
	
endif(WIN32)
