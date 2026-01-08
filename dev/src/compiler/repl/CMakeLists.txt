add_library(Repl STATIC
    src/repl/repl_frontend.cpp
    src/repl/repl_session.cpp
    src/repl/dvm_helpers.cpp
)

target_include_directories(Repl PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../driver/driver/src_private
)

make_module(Repl USES Base
        Filesystem
        QueryFramework
        Driver
        HELIOS
        ModuleTree)
