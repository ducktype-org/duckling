add_library(Formatter STATIC
	src/formatter/config.cpp
	src/formatter/doc.cpp
	src/formatter/doc_builder.cpp
	src/formatter/doc_renderer.cpp
	src/formatter/formatter.cpp
	src/formatter/source_text.cpp
	src/formatter/spacing.cpp
	src/formatter/statement_tree.cpp
	src/formatter/token_classes.cpp
)

make_module(Formatter USES Lexer nlohmann_json::nlohmann_json)

duck_add_test(formatter formatter tests/formatter_test.cpp USES Formatter TokenSource nlohmann_json::nlohmann_json)

# duckfmt: the standalone command-line formatter built on top of the Formatter library.
add_subdirectory(format_main)
