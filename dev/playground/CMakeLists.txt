# Playground
add_executable(panic_playground make_panic.cpp)
add_executable(clap_playground clap_playground.cpp)
add_executable(clap_cat clap_cat.cpp)

target_link_libraries(panic_playground Base)
target_link_libraries(clap_playground Base Clap Printer)
target_link_libraries(clap_cat Base Clap Printer)
