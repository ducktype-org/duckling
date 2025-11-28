
add_subdirectory(core)
add_subdirectory(dia_templates)
add_subdirectory(term_ui)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -g3") # TODO: This should be more isolated
# Optional HELIOS/UI-dependent interactive diagnostics in a separate library
add_library(DiagnosticInteractive STATIC
        src/diagnostic_interactive/diagnostic.cpp
        src/diagnostic_interactive/logger.cpp
)

make_module(DiagnosticInteractive USES TypeSystemHigher Printer Filesystem TokenSource TerminalUI DiagnosticInteractiveCore Diagnostic )
