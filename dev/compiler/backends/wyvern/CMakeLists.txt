add_library(WyvernBackend
        src/backends/wyvern/detail/wyvern_lowering.cpp
        src/backends/wyvern/detail/module_impl.cpp
)

llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Support
    nativecodegen)
target_include_directories(WyvernBackend PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(WyvernBackend ${llvm_libs})

make_module(WyvernBackend USES Base QueryFramework LIR llvm_helpers)

duck_add_test(compiler wyvern_test tests/wyvern_test.cpp
	USES Base Tester WyvernBackend)
