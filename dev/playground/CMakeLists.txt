# Playground
add_executable(panic_playground make_panic.cpp)
add_executable(clap_playground clap_playground.cpp)

target_link_libraries(panic_playground Base)
target_link_libraries(clap_playground Base Clap Printer)
