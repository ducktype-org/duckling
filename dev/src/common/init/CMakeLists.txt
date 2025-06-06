add_library(Init
        src/init/init.cpp
)

make_module(Init USES Base)

duck_add_test(common init tests/init_test.cpp USES Base Init)
