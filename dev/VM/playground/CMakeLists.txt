add_executable(parser_f8_testing parser_f8_testing.cpp)

target_include_directories(parser_f8_testing PRIVATE ../src)

target_link_libraries(parser_f8_testing LibVm Base Listener Clap Crow::Crow Filesystem TPC)

