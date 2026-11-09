set(SOURCES
    src/system_command/system_command.cpp
)

# @TODO: #3746 Look at the history of https://github.com/ducktype-org/duckling/pull/3735 for the PoC Windows implementation.
#        Although cpp-linter fails on it (I think it runs on Ubuntu, that's why).
if(UNIX)
    list(APPEND SOURCES src/system_command/system_command_unix.cpp)
else()
    message(FATAL_ERROR "System Command: Unsupported operating system: ${CMAKE_SYSTEM_NAME}")
endif()

add_library(SystemCommand
    ${SOURCES}
)

make_module(SystemCommand USES Base StringID Logger OSUtils)

duck_add_test(common system_command tests/system_command_test.cpp USES SystemCommand Base)
