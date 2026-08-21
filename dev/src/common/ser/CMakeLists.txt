add_library(Ser
	src/ser/ser.cpp
)

make_module(Ser USES Base)

duck_add_test(common ser tests/ser_test.cpp USES Ser Base)

duck_add_test(common ser_base tests/ser_base_test.cpp USES Ser Base)
