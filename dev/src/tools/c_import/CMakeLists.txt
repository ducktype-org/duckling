add_library(CImport
	src/c_import/emit.cpp
	src/c_import/identifiers.cpp
	src/c_import/lower.cpp
	src/c_import/request.cpp
	src_private/c_import_private/tu_reader.cpp
)

make_module(CImport USES Base JSON LexerDefinitions StringID)
target_link_libraries(CImport duck_libclang)

add_executable(duck_c_import main/main.cpp)
target_link_libraries(duck_c_import CImport Init)
target_compile_definitions(duck_c_import PRIVATE DUCK_CLANG_RESOURCE_DIR="${DUCK_CLANG_RESOURCE_DIR}")

duck_add_test(tools c_import_lower tests/c_import_lower_test.cpp USES CImport)
