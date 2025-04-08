add_library(DVMBackend
    src_private/backend.cpp
)

make_module(DVMBackend USES Base QueryFramework LIR LibVM)
