add_library(CTV STATIC
    src/ctv/ctv.cpp
)

# TODOP: CTV tests
make_module(CTV USES Base TypeSystemHigher)