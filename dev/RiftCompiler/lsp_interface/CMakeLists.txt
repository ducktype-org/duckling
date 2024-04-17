add_executable(lsptree_interface src/lsp_interface/lsptree_interface.cpp)
add_library(LSP
		src/lsp_interface/export_keywords.cpp)
target_link_libraries(lsptree_interface Lexer PstParser)
make_module(LSP USES Base Lexer)
