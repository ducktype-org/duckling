add_library(
    DiagnosticCore
    src/diagnostic/core/diagnostic_arguments.cpp
    src/diagnostic/core/template_evaluation.cpp
    src/diagnostic/core/template_registry.cpp
    src/diagnostic/core/template_file.cpp
    src/diagnostic/core/diagnostic_state.cpp
    src/diagnostic/core/view_constructors.cpp
)

make_module(DiagnosticCore USES JSON yaml-cpp::yaml-cpp Base DiaTemplates Filesystem Concurrent)
target_include_directories(DiagnosticCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

duck_add_test(common dia_interactive_unit tests/dia_interactive_unit_test.cpp USES DiagnosticCore)
duck_add_test(common dia_interactive_e2e tests/dia_interactive_e2e_test.cpp USES DiagnosticCore)
