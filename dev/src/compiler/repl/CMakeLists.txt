add_library(Repl STATIC
    src/repl/repl_session.cpp
)

target_include_directories(Repl PUBLIC src)

target_link_libraries(Repl 
    PUBLIC
        Base
        Filesystem
        QueryFramework
        Driver
        HELIOS
        ModuleTree
)
