
# @TODO: this is frigile
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/doc-config/cmake" ${CMAKE_MODULE_PATH})

find_package(Doxygen REQUIRED)
find_package(Sphinx REQUIRED)

# Find all the public headers and source files
# TEMPORARY SOLUTION (Nothing is as permanent as a temporary solution)
file(GLOB_RECURSE DUCKLING_PP_FILES ${PROJECT_SOURCE_DIR}/*.*pp ${PROJECT_SOURCE_DIR}/*.md)
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

set(SPHINX_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/sphinx)
set(SPHINX_INDEX_FILE ${SPHINX_BUILD}/index.html)

file(GLOB_RECURSE SPHINX_RST_FILES ${SPHINX_SOURCE}/*.rst)
list(REMOVE_ITEM SPHINX_RST_FILES */.venv/*)
list(REMOVE_ITEM SPHINX_RST_FILES .venv/*)

# Only regenerate Sphinx when:
# - Doxygen has rerun
# - Our doc files have been updated
# - The Sphinx config has been updated
add_custom_command(OUTPUT ${SPHINX_INDEX_FILE}
                   COMMAND
                   ${SPHINX_EXECUTABLE} -b html
                   ${SPHINX_SOURCE} ${SPHINX_BUILD}
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   DEPENDS
                   ${SPHINX_RST_FILES}
                   ${DOXYGEN_INDEX_FILE}
                   MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
                   COMMENT "Generating documentation with Sphinx")

add_custom_target(docs DEPENDS ${DOXYGEN_INDEX_FILE} ${SPHINX_INDEX_FILE})

add_custom_target(doxygen-docs DEPENDS ${DOXYGEN_INDEX_FILE})
add_custom_target(sphinx-docs DEPENDS ${SPHINX_INDEX_FILE})


# Bulding PDF files is done in two steps. First, we generate the latex files
# and then we compile them with latexmk. To combine the two steps, we use "latexpdf" builder.
# Documentation: https://www.sphinx-doc.org/en/master/usage/builders/index.html#sphinx.builders.latex.LaTeXBuilder
# Required dependencies:
# sudo apt install latexmk texlive-latex-recommended texlive-fonts-recommended texlive-latex-extra texlive-fonts-extra tex-gyre
# Additional dependencies in the documentation.
add_custom_target(sphinx-docs-pdf
                    COMMAND
                    ${SPHINX_EXECUTABLE} -M latexpdf ${SPHINX_SOURCE} ${SPHINX_BUILD}
                    COMMAND
                    ${CMAKE_COMMAND} -E echo "Generated PDF documentation is in ${SPHINX_BUILD}/latex"
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                    DEPENDS
                    ${SPHINX_RST_FILES}
                    ${SPHINX_SOURCE}/conf.py
                    COMMENT "Generating PDF documentation with Sphinx")

set_property(TARGET docs APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${DOXYGEN_OUTPUT_DIR} ${SPHINX_BUILD})

set_property(TARGET doxygen-docs APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${DOXYGEN_OUTPUT_DIR})
set_property(TARGET sphinx-docs APPEND
            PROPERTY ADDITIONAL_CLEAN_FILES ${SPHINX_BUILD})

add_custom_target(clean-docs
                    COMMAND echo "Removing ${CMAKE_CURRENT_BINARY_DIR}/{doxygen/*,sphinx,html,latex}"
                    COMMAND /bin/sh -c "rm -rf ${CMAKE_CURRENT_BINARY_DIR}/doxygen/* ${CMAKE_CURRENT_BINARY_DIR}/sphinx ${CMAKE_CURRENT_BINARY_DIR}/html ${CMAKE_CURRENT_BINARY_DIR}/latex"
                    USES_TERMINAL
                    VERBATIM)

add_custom_target(open-sphinx-docs
                    COMMAND xdg-open ${SPHINX_INDEX_FILE}
                    VERBATIM)

add_custom_target(open-doxygen-docs
                    COMMAND xdg-open ${DOXYGEN_OUTPUT_DIR}/html/index.html
                    VERBATIM)
