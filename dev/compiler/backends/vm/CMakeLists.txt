add_library(DVMBackend
    src/backends/vm/backend.cpp
    src/backends/vm/backend.hpp
)


make_module(DVMBackend USES Base QueryFramework LIR Init ${libvm})
