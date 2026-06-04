add_library(Diagnostic STATIC
        src/diagnostic/source_position.cpp
        src/diagnostic/location.cpp
        src/diagnostic/highlight_positions.cpp
)

make_module(Diagnostic USES Printer Filesystem TokenSource)
