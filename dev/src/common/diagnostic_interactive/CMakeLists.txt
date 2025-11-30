
add_subdirectory(core)
add_subdirectory(dia_templates)
add_subdirectory(term_ui)

add_library(DiagnosticInteractive STATIC
        src/diagnostic_interactive/message.cpp
        src/diagnostic_interactive/logger.cpp
)
make_module(DiagnosticInteractive USES Printer Filesystem TokenSource TerminalUI DiagnosticInteractiveCore Diagnostic )
