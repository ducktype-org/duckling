add_library(Tester
	src/tester/tester.cpp
	src/tester/tester_config.cpp
)

make_module(Tester USES Printer Base Clap)
target_compile_definitions(Tester PRIVATE CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_example(tester_example examples/example.cpp USES Tester Printer)

rift_add_test(common tester_test_simple tests/test.cpp
	USES)
