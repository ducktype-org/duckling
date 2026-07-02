add_library(libquerylinger
    querylinger/queries/version.hpp
    querylinger/queries/version.cpp
)

target_include_directories(libquerylinger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libquerylinger PUBLIC Base QueryFramework)
target_link_libraries(libquerylinger PRIVATE Logger Concurrent)

add_example(querylinger main.cpp USES libquerylinger Base Init Clah Logger)

duck_add_test(compiler querylinger tests/querylinger_test.cpp
    USES libquerylinger QueryFramework Concurrent Base Init Logger)
