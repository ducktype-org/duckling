add_executable(lsp_daemon src/lsp_interface/lsp_daemon.cpp
                          src/lsp_interface/export_keywords.cpp
                          src/lsp_interface/semantic_tokens.cpp
                          src/lsp_interface/utils.cpp)

target_link_libraries(lsp_daemon PUBLIC base64 Base Lexer PstParser Crow Clap LibVM)
