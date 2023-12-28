set(DOXYGEN_EXTRACT_ALL YES)
set(DOXYGEN_BUILTIN_STL_SUPPORT YES)

#doxygen_add_docs(docs 
#    ViennaVulkanEngine/vve.h "${CMAKE_CURRENT_SOURCE_DIR}/mainpage.md"
#    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/include"
#)

set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "mainpage.md")
doxygen_add_docs(doxygen
    ${CMAKE_SOURCE_DIR}/README.md
    ${CMAKE_SOURCE_DIR}/include/ViennaVulkanEngine
    ${CMAKE_SOURCE_DIR}/src
)

