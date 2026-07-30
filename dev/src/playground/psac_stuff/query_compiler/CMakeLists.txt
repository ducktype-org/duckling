add_library(libquerylinger
    querylinger/queries/inputs.hpp
    querylinger/queries/inputs.cpp
    querylinger/queries/codegen.hpp
    querylinger/queries/codegen.cpp
    querylinger/session.hpp
    querylinger/session_state.hpp
    querylinger/session.cpp
)

target_include_directories(libquerylinger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libquerylinger PUBLIC Base QueryFramework Hashing libpsackling)
target_link_libraries(libquerylinger PRIVATE Logger Concurrent PstParser)

add_example(querylinger main.cpp USES libquerylinger Base Init Clah Logger)

duck_add_test(compiler querylinger tests/querylinger_test.cpp
    USES libquerylinger psack_corpus QueryFramework Concurrent Base Init Logger SystemCommand)
