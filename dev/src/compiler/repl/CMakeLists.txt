add_library(Repl
    src/repl/repl_frontend.cpp
    src/repl/repl_session.cpp
    src/repl/utils.cpp
)

make_module(Repl USES Base
        Filesystem
        QueryFramework
        Driver
        HELIOS
        ModuleTree)
