add_library(
    DiagnosticInteractiveCore
    src/diagnostic_interactive/core/diagnostic_file.cpp
    src/diagnostic_interactive/core/template_evaluation.cpp
    src/diagnostic_interactive/core/template_file.cpp
    src/diagnostic_interactive/core/thread_state.cpp
)

make_module(DiagnosticInteractiveCore USES JSON yaml-cpp::yaml-cpp Base DiaTemplates Filesystem)

duck_add_test(common diagnostic_interactive tests/diagnostic_interactive_test.cpp USES DiagnosticInteractiveCore Base JSON yaml-cpp::yaml-cpp)