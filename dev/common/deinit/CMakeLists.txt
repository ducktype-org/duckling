add_library(Deinit
        src/deinit/deinit.cpp
)

make_module(Deinit USES Base)

duck_add_test(common deinit_test tests/test.cpp
	USES Base Deinit)
