add_library(DVMBackend
    src/backends/vm/backend.cpp
    src/backends/vm/builders.cpp
    src/backends/vm/serializer.cpp
)


make_module(DVMBackend USES Base QueryFramework LIR Init LibVM_tc)
