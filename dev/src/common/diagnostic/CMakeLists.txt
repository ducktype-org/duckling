add_library(Diagnostic STATIC
        src/diagnostic/source_position.cpp
        src/diagnostic/logger.cpp
        src/diagnostic/diagnostic_converters.cpp
        src/diagnostic/location.cpp
        src/diagnostic/highlight_positions.cpp
)

# Core Diagnostic library without HELIOS/UI dependencies
make_module(Diagnostic USES Printer Filesystem TokenSource JSON)

# Optional HELIOS/UI-dependent interactive diagnostics in a separate library
add_library(DiagnosticInteractive STATIC
        src/diagnostic/interactive_message.cpp
        src/diagnostic/interactive_logger.cpp
        src/diagnostic/interactive_logger_demo.cpp
        src/diagnostic/interactive_code.cpp
)

make_module(DiagnosticInteractive USES Printer Filesystem TokenSource JSON HELIOS VIEW_MANAGER TERM_UI Diagnostic)
