add_library(DebugInfo
        src/debug_info/debug_info.cpp
        src/debug_info/debug_info_builder.cpp
        src/debug_info/debug_info_io.cpp
)

make_module(DebugInfo USES Base Ser)
target_include_directories(DebugInfo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

duck_add_test(common debug_info tests/debug_info_test.cpp USES DebugInfo Base)
