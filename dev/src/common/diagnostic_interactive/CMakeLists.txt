# Optional HELIOS/UI-dependent interactive diagnostics in a separate library
add_library(DiagnosticInteractive STATIC
        src/diagnostic/interactive_message.cpp
        src/diagnostic/interactive_logger.cpp
        src/diagnostic/interactive_code.cpp
)

make_module(DiagnosticInteractive USES Printer Filesystem TokenSource JSON HELIOS_SYMBOLS VIEW_MANAGER TERM_UI Diagnostic TypeSystemHigher)
