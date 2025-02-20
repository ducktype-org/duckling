add_library(Init
        src/init/init.cpp
)

make_module(Init USES Base)

duck_add_test(common init_test tests/test.cpp
	USES Base Init)
