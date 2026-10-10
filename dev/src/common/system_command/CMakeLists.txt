set(SOURCES
    src/system_command/system_command.cpp
)

if(WIN32)
    list(APPEND SOURCES src/system_command/system_command_windows.cpp)
elseif(UNIX)
    list(APPEND SOURCES src/system_command/system_command_unix.cpp)
else()
    message(FATAL_ERROR "Unsupported operating system: ${CMAKE_SYSTEM_NAME}")
endif()

add_library(SystemCommand
    ${SOURCES}
)

make_module(SystemCommand USES Base StringID Logger OSUtils)

duck_add_test(common system_command tests/system_command_test.cpp USES SystemCommand Base)
