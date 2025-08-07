add_library(ctv STATIC
    src/ctv/num_ctv.cpp
)

make_module(ctv USES Base)
