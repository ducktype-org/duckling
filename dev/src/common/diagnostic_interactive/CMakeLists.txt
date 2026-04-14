
add_subdirectory(core)
add_subdirectory(dia_templates)
add_subdirectory(term_ui)
add_subdirectory(lsp_ui)

add_library(DiagnosticInteractive STATIC
        src/diagnostic_interactive/message.cpp
        src/diagnostic_interactive/logger.cpp
        src/diagnostic_interactive/hash_source_position.cpp
        src/diagnostic_interactive/placeholder.cpp
        src/diagnostic_interactive/module_flags/module_flags.cpp
)
make_module(DiagnosticInteractive USES  TerminalUI DiagnosticInteractiveCore Diagnostic Logger)


add_example(diagnostic_arguments_evaluator examples/diagnostic_arguments_evaluator.cpp USES DiagnosticInteractive)
add_example(dia_usage_example examples/dia_usage_example.cpp USES DiagnosticInteractive)

