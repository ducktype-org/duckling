add_library(psack_corpus compiler_test_corpus.cpp compiler_test_corpus.hpp)

target_include_directories(psack_corpus PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(psack_corpus PUBLIC Base Filesystem)
