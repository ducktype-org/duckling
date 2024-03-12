add_library(LSP
		src/lsp_interface/export_keywords.cpp)

make_module(LSP USES Base Lexer)
