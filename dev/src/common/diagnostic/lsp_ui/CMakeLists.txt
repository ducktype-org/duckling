add_library(DiagnosticLspUI
    src/diagnostic/lsp_ui/lsp_ui.cpp
)

make_module(DiagnosticLspUI USES DiagnosticCore Base lsp)
