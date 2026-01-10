

add_library(DiagnosticLspUI
    src/diagnostic_interactive/lsp_ui/diagnostic.cpp
)

make_module(DiagnosticLspUI USES DiagnosticInteractiveCore Base)

