add_library(Printer
	src/printer/stream_printer.cpp
)

make_module(Printer USES Base)

add_example(printer_example examples/printer_example.cpp USES Printer)

duck_add_test(common printer tests/printer_test.cpp USES Printer)
