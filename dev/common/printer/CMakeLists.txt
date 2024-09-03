add_library(Printer
	src/printer/stream_printer.cpp
)

make_module(Printer USES Base)

add_example(printer_example examples/example.cpp USES Printer)

cmake_add_test(common printer_test_simple tests/printer_tests.cpp
	USES Printer)
