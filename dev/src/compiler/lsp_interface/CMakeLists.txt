add_library(LspInterface
	src/lsp_interface/files_cache.cpp
	src/lsp_interface/server_session.cpp
	src/lsp_interface/compiler.cpp
)

make_module(LspInterface USES Base Init Lexer PstParser Clah HELIOS QueryFramework
	DiagnosticLspUI Driver GlobalState lsp)

add_executable(duck_ls src/lsp_interface/main.cpp)
target_link_libraries(duck_ls PUBLIC LspInterface)

duck_add_test(compiler lsp_server tests/lsp_server_test.cpp USES LspInterface)
