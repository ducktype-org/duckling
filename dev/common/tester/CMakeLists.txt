add_library(Tester
        src/tester/tester.cpp
        src/tester/tester_config.cpp)

make_module(Tester USES Printer Base Clap)

add_example(tester_example examples/example.cpp USES Tester Printer)

rift_add_test(common tester_test_simple tests/test.cpp
        USES)
