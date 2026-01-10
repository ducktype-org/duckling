

add_library(DiagnosticLspUI
    src/diagnostic_interactive/lsp_ui/lsp_ui.cpp
)

make_module(DiagnosticLspUI USES DiagnosticInteractiveCore Base)

