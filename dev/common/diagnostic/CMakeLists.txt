add_library(Diagnostic
        src/diagnostic/source_position.cpp
        src/diagnostic/logger.cpp
        src/diagnostic/diagnostic_converters.cpp
        src/diagnostic/interactive_message.cpp
        src/diagnostic/interactive_logger.cpp
        src/diagnostic/interactive_code.cpp
        src/diagnostic/location.cpp
        src/diagnostic/highlight_positions.cpp
)

make_module(Diagnostic USES Printer Filesystem TokenSource JSON HELIOS)
