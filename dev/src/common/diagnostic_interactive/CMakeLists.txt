
add_subdirectory(core)
add_subdirectory(dia_templates)
add_subdirectory(term_ui)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -g3") # TODO: This should be more isolated
# Optional HELIOS/UI-dependent interactive diagnostics in a separate library
add_library(DiagnosticInteractive STATIC
        src/diagnostic/interactive_message.cpp
        src/diagnostic/interactive_logger_demo.cpp
        src/diagnostic/interactive_code.cpp
)

make_module(DiagnosticInteractive USES Printer Filesystem TokenSource JSON HELIOS_SYMBOLS TerminalUI Diagnostic TypeSystemHigher)
