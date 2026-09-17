add_library(LspInterface
	src/lsp_interface/files_cache.cpp
	src/lsp_interface/server_session.cpp
	src/lsp_interface/compiler.cpp
)

make_module(LspInterface USES Base Init Lexer PstParser Clah Version HELIOS QueryFramework
	DiagnosticLspUI Driver GlobalState lsp)

add_executable(duck_ls src/lsp_interface/main.cpp)
target_link_libraries(duck_ls PUBLIC LspInterface)

duck_add_test(compiler lsp_files_cache tests/lsp_files_cache_test.cpp USES LspInterface)
duck_add_test(compiler lsp_server tests/lsp_server_test.cpp USES LspInterface)
duck_add_test(compiler lsp_session tests/lsp_session_test.cpp USES LspInterface)
