find_package(Doxygen REQUIRED)

# Find all the public headers and source files
# TEMPORARY SOLUTION (Nothing is as permanent as a temporary solution)
file(GLOB_RECURSE DUCKLING_PP_FILES ${PROJECT_SOURCE_DIR}/*.*pp ${PROJECT_SOURCE_DIR}/*.md ${PROJECT_SOURCE_DIR}/*.dox)
list(REMOVE_ITEM DUCKLING_PP_FILES */.venv/*)
list(REMOVE_ITEM DUCKLING_PP_FILES */*build*/*)

set(DOXYGEN_INPUT_DIR ${PROJECT_SOURCE_DIR})
set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# Replace variables inside @@ with the current values
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

# Doxygen won't create this for us
file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})

# Only regenerate Doxygen when the Doxyfile or public headers change
add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
                   DEPENDS ${DUCKLING_PP_FILES}
                   COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
                   MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
                   COMMENT "Generating docs"
                   VERBATIM)

add_custom_target(docs DEPENDS ${DOXYGEN_INDEX_FILE})

add_custom_target(doxygen-docs DEPENDS ${DOXYGEN_INDEX_FILE})

set_property(TARGET docs APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${DOXYGEN_OUTPUT_DIR})

set_property(TARGET doxygen-docs APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${DOXYGEN_OUTPUT_DIR})

add_custom_target(clean-docs
                    COMMAND echo "Removing ${CMAKE_CURRENT_BINARY_DIR}/{doxygen/*,html}"
                    COMMAND /bin/sh -c "rm -rf ${CMAKE_CURRENT_BINARY_DIR}/doxygen/* ${CMAKE_CURRENT_BINARY_DIR}/html"
                    USES_TERMINAL
                    VERBATIM)

add_custom_target(open-doxygen-docs
                    COMMAND xdg-open ${DOXYGEN_OUTPUT_DIR}/html/index.html
                    VERBATIM)
