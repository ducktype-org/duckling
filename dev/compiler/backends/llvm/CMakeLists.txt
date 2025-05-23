add_library(LLVMBackend
    src_private/backends/llvm_private/llvm_lowering.cpp
    src_private/backends/llvm_private/compile_llvm.cpp
    src_private/backends/llvm_private/module.cpp
    src_private/backends/llvm_private/get_parameter_types.cpp
)

execute_process(COMMAND llvm-config --libs Core ExecutionEngine Interpreter MC MCJIT Support nativecodegen
                OUTPUT_VARIABLE llvm_libs
                OUTPUT_STRIP_TRAILING_WHITESPACE)

target_include_directories(LLVMBackend PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(LLVMBackend ${llvm_libs})

make_module(LLVMBackend USES Base QueryFramework LIR llvm_helpers Init)

duck_add_test(compiler llvm tests/llvm_test.cpp USES Base Tester LLVMBackend)
