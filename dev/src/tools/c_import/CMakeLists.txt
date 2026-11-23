add_library(CImport
	src/c_import/commands.cpp
	src/c_import/doc_comment.cpp
	src/c_import/emit.cpp
	src/c_import/generate.cpp
	src/c_import/identifiers.cpp
	src/c_import/lower.cpp
	src/c_import/package.cpp
	src/c_import/process.cpp
	src/c_import/recipe.cpp
	src/c_import/resolve.cpp
	src/c_import/verify.cpp
	src_private/c_import_private/tu_reader.cpp
)

make_module(CImport USES Base LexerDefinitions StringID yaml-cpp::yaml-cpp)
target_link_libraries(CImport duck_libclang)
target_compile_definitions(CImport PRIVATE DUCK_CLANG_RESOURCE_DIR="${DUCK_CLANG_RESOURCE_DIR}")

add_executable(duckbind main/main.cpp)
target_link_libraries(duckbind CImport Clah Base Init)

duck_add_test(tools c_import_lower tests/c_import_lower_test.cpp USES CImport)
duck_add_test(tools c_import_package tests/c_import_package_test.cpp USES CImport)
duck_add_test(tools c_import_commands tests/c_import_commands_test.cpp USES CImport OSUtils)
