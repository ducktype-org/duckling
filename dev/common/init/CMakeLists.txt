add_library(Init
        src/init/init.cpp
)

make_module(Init USES Base)

duck_add_test(common init USES Base Init)
