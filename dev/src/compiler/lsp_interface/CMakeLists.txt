add_library(LspInterface
	src/lsp_interface/uri_conversion.cpp
	src/lsp_interface/server_session.cpp
	src/lsp_interface/handlers.cpp
	src/lsp_interface/compiler_files_management.cpp
	src/lsp_interface/text_sync.cpp
	src/lsp_interface/open_documents.cpp
	src/lsp_interface/export_keywords.cpp
	src/lsp_interface/semantic_tokens.cpp
	src/lsp_interface/go_to_definition.cpp
	src/lsp_interface/diagnostics.cpp
	src/lsp_interface/utils.cpp
)

make_module(LspInterface USES Base Init Lexer PstParser Clah HELIOS QueryFramework
	DiagnosticLspUI Driver GlobalState lsp)

add_executable(duck_ls src/lsp_interface/server_main.cpp)
target_link_libraries(duck_ls PUBLIC LspInterface)

duck_add_test(compiler lsp_server tests/lsp_server_test.cpp USES LspInterface)
