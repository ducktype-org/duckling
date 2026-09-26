add_library(CImport
	src/c_import/c_decls.cpp
	src/c_import/identifiers.cpp
	src/c_import/type_mapper.cpp
	src/c_import/dk_emitter.cpp
	src/c_import/manifest_emitter.cpp
	src/c_import/package_writer.cpp
	src/c_import/options.cpp
	src/c_import/run.cpp
	src/c_import/verify.cpp
	src_private/c_import_private/tu_reader.cpp
)

# libclang's headers are marked SYSTEM so the C API does not take part in linting.
target_include_directories(CImport SYSTEM PRIVATE ${CLANG_INCLUDE_DIRS})

# make_module links with the plain signature, so every link here must stay plain too:
# CMake forbids mixing plain and keyword signatures on one target.
target_link_libraries(CImport libclang)

# libclang cannot locate its own builtin headers when loaded as a library.
target_compile_definitions(CImport PRIVATE DUCK_CLANG_RESOURCE_DIR="${DUCK_CLANG_RESOURCE_DIR}")

make_module(CImport USES Base)

add_executable(duck_c_import src/c_import/main.cpp)

target_link_libraries(duck_c_import CImport Base Clah Printer Version)

duck_add_test(compiler c_import_identifiers tests/c_import_identifiers_test.cpp USES Base Tester CImport)
duck_add_test(compiler c_import_types tests/c_import_types_test.cpp USES Base Tester CImport)
duck_add_test(compiler c_import_emitter tests/c_import_emitter_test.cpp USES Base Tester CImport)
duck_add_test(compiler c_import_tu tests/c_import_tu_test.cpp USES Base Tester CImport)
duck_add_test(compiler c_import_package tests/c_import_package_test.cpp USES Base Tester CImport)
