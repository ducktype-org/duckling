add_library(Diagnostic STATIC
        src/diagnostic/source_position.cpp
        src/diagnostic/logger.cpp
        src/diagnostic/diagnostic_converters.cpp
        src/diagnostic/location.cpp
        src/diagnostic/highlight_positions.cpp
)

# Core Diagnostic library without HELIOS/UI dependencies
make_module(Diagnostic USES Printer Filesystem TokenSource JSON)
