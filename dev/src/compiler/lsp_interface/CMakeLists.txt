add_executable(duck_ls src/lsp_interface/main.cpp
                          src/lsp_interface/file_changed.cpp
                          src/lsp_interface/export_keywords.cpp
                          src/lsp_interface/semantic_tokens.cpp
                          src/lsp_interface/go_to_definition.cpp
                          src/lsp_interface/validation.cpp
                          src/lsp_interface/utils.cpp)

target_link_libraries(duck_ls PUBLIC base64 Base Lexer PstParser Crow Clah HELIOS QueryFramework DiagnosticLspUI)
