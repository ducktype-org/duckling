add_executable(lsptree_interface src/lsp_interface/lsptree_interface.cpp)
add_executable(export_keywords src/lsp_interface/main.cpp src/lsp_interface/export_keywords.cpp)

target_link_libraries(lsptree_interface Lexer PstParser)
target_link_libraries(export_keywords Base Lexer PstParser Clap)
