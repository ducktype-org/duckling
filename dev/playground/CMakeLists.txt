# Playground
add_executable(panic_playground make_panic.cpp)
add_executable(clap_playground clap_playground.cpp)
add_executable(clap_example_cat ../docs/source-doc/source-docs/common/clap/clap_example_cat.cpp)
add_executable(fs_parser_playground fs_parser_playground.cpp)

target_link_libraries(panic_playground Base)
target_link_libraries(clap_playground Base Clap Printer)
target_link_libraries(clap_example_cat Base Clap)
target_link_libraries(fs_parser_playground Base Frontend)
