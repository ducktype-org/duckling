add_library(PstParser
		src/pst_parser/parser.cpp
		src/pst_parser/elements/elements.cpp
		src/pst_parser/elements/elements_impl.cpp
		src/pst_parser/pst.cpp
		src/pst_parser/rift_parser_base.cpp)

add_library(ModuleSystem
		src/module_system/import_to_path.cpp)

add_library(SymbolTable
		src/hir/symtable/symtable.cpp
		src/hir/symtable/scope.cpp
		src/hir/symtable/symbol.cpp
		src/hir/symtable/lookup_result.cpp)

add_library(TypeSystem
		src/typesystem/class_types.cpp
		src/typesystem/type_info.cpp
		src/typesystem/implicit_coercibility.cpp
		src/typesystem/typesystem.cpp
		src/typesystem/internal/type_info_impl.cpp
		src/typesystem/internal/class_info_impl.cpp)

add_library(Compiler
		src/compiler/compiler_config.cpp
		src/compiler/compilation_handler.cpp)

add_library(HIR
		src/hir/hir.cpp
		src/hir/rift_symbols.cpp
		src/hir/analysis_state.cpp
		src/hir/hir_expr.cpp)

add_library(Exec
		src/exec/exec.cpp
		src/exec/ctv.cpp
		src/exec/operators/builtinoperators.cpp
		src/exec/vtable_creation.cpp
		src/exec/exec_default.cpp)

add_library(Operations
		src/operations/operation.cpp
		src/operations/create_default.cpp
		src/operations/internal/builtin_operations.cpp)


add_library(LSP
	src/lsp_interface/export_keywords.cpp
)

# @FIXME: making modules like most likely allow importing from entire dev/src:
# make modules:
make_module(PstParser USES Lexer Printer TPC)
make_module(ModuleSystem USES Base PstParser)
make_module(SymbolTable USES Base PstParser)
make_module(Compiler USES Base Printer Clap PstParser ModuleSystem SymbolTable TypeSystem)
make_module(HIR USES Base SymbolTable PstParser TypeSystem)
make_module(TypeSystem USES Base SymbolTable Exec)
make_module(Exec USES Base Operations)
make_module(Operations USES TypeSystem Exec Base)
make_module(LSP USES Base Lexer)

# Tests:

rift_add_test(compiler symtable_test_simple src/hir/symtable/tests/symtable_test_simple.cpp
		USES SymbolTable PstParser)
rift_add_test(compiler parser_test_simple src/pst_parser/tests/test.cpp
		USES SymbolTable PstParser)
rift_add_test(compiler module_system_test_simple src/module_system/tests/test.cpp
		USES Printer ModuleSystem PstParser Lexer)
rift_add_test(compiler compiler_test_simple src/compiler/tests/test.cpp
		USES Printer ModuleSystem PstParser Lexer Compiler)
rift_add_test(compiler typesystem_test_simple src/typesystem/tests/simple_test.cpp
		USES Base Clap SymbolTable TypeSystem)
rift_add_test(compiler typesystem_test_new src/typesystem/tests/new_test.cpp
		USES Base Clap SymbolTable TypeSystem)
rift_add_test(compiler exec_ctv_test_simple src/exec/tests/simple_ctv_test.cpp
		USES Exec Clap TypeSystem)
rift_add_test(compiler exec_defaults_test_simple src/exec/tests/simple_defaults_test.cpp
		USES Exec Clap TypeSystem)
rift_add_test(compiler exec_constructor_test_simple src/exec/tests/simple_constructor_test.cpp
		USES Exec Clap TypeSystem)
rift_add_test(compiler operations_test_simple src/operations/tests/simple_test.cpp
		USES Base Clap Operations TypeSystem)
rift_add_test(compiler operation_constructor src/operations/tests/constructors_test.cpp
		USES Base Clap Operations TypeSystem)
rift_add_test(compiler exec_operators_test_simple src/exec/tests/operators_test.cpp
		USES Exec Base Clap Operations TypeSystem)
rift_add_test(compiler hir_test_simple src/hir/tests/test.cpp
		USES Exec Base Clap Operations TypeSystem TPC PstParser HIR)

# Main:
add_executable(rift_main src/main.cpp)
target_link_libraries(rift_main Compiler Printer Lexer PstParser Filesystem Clap Base)

add_subdirectory(playground)
