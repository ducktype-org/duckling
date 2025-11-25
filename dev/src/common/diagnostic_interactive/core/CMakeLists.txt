add_library(
    DiagnosticInteractiveCore
    src/diagnostic_interactive/core/diagnostic_file.cpp
    src/diagnostic_interactive/core/template_evaluation.cpp
    src/diagnostic_interactive/core/template_registry.cpp
    src/diagnostic_interactive/core/template_file.cpp
    src/diagnostic_interactive/core/diagnostic_state.cpp
)

make_module(DiagnosticInteractiveCore USES JSON yaml-cpp::yaml-cpp Base DiaTemplates Filesystem)

duck_add_test(common dia_interactive_unit tests/dia_interactive_unit_test.cpp USES DiagnosticInteractiveCore Base JSON yaml-cpp::yaml-cpp)
duck_add_test(common dia_interactive_e2e tests/dia_interactive_e2e_test.cpp USES DiagnosticInteractiveCore Base JSON yaml-cpp::yaml-cpp)