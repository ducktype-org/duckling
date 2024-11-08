add_library(LLVMBackend
        src/backends/llvm/detail/llvm_lowering.cpp
        src/backends/llvm/detail/module_impl.cpp
)

llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Support
    nativecodegen)
target_include_directories(LLVMBackend PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(LLVMBackend ${llvm_libs})

make_module(LLVMBackend USES Base QueryFramework LIR llvm_helpers)

