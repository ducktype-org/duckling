add_library(Listener INTERFACE)

target_include_directories(Listener INTERFACE src)

duck_add_test(common listener tests/listener_test.cpp USES Listener)
