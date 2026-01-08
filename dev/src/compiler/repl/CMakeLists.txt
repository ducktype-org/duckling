add_library(Repl STATIC
    src/repl/repl_frontend.cpp
    src/repl/repl_session.cpp
)

make_module(Repl USES Base
        Filesystem
        QueryFramework
        Driver
        HELIOS
        ModuleTree)
