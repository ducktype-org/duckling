
add_subdirectory(core)
add_subdirectory(dia_templates)
add_subdirectory(term_ui)
add_subdirectory(lsp_ui)

add_library(Diagnostic STATIC
        src/diagnostic/message.cpp
        src/diagnostic/logger.cpp
        src/diagnostic/stable_position.cpp
        src/diagnostic/placeholder.cpp
        src/diagnostic/module_flags/module_flags.cpp
        src/diagnostic/source_position.cpp
        src/diagnostic/location.cpp
        src/diagnostic/highlight_positions.cpp
)
make_module(Diagnostic USES TerminalUI DiagnosticCore Logger Printer Filesystem TokenSource)


add_example(diagnostic_arguments_evaluator examples/diagnostic_arguments_evaluator.cpp USES Diagnostic)
add_example(dia_usage_example examples/dia_usage_example.cpp USES Diagnostic)

