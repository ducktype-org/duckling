add_library(LLVMBackend
    src_private/backends/llvm_private/llvm_lowering.cpp
    src_private/backends/llvm_private/call_lowering.cpp
    src_private/backends/llvm_private/abi_converter.cpp
    src_private/backends/llvm_private/compile_llvm.cpp
    src_private/backends/llvm_private/module.cpp
    src_private/backends/llvm_private/module_impl.cpp
)

llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Passes
    Support
    nativecodegen)
target_include_directories(LLVMBackend PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(LLVMBackend ${llvm_libs})

make_module(LLVMBackend USES Base GlobalState QueryFramework LIR llvm_helpers Init Logger)

duck_add_test(compiler llvm tests/llvm_test.cpp USES Base Tester LLVMBackend)
