add_library(Diagnostic
        src/diagnostic/source_position.cpp
        src/diagnostic/error_state.cpp
        src/diagnostic/message.cpp
)

make_module(Diagnostic USES Printer Filesystem)
