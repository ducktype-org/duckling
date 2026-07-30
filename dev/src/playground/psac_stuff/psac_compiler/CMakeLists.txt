add_library(libpsackling
    psackling/compiler.hpp
    psackling/compiler.cpp
    psackling/llvm.hpp
    psackling/llvm.cpp
    psackling/type.hpp
    psackling/symbol.hpp
    psackling/global_pass.hpp
    psackling/global_pass.cpp
    psackling/function_pass.hpp
    psackling/function_pass.cpp
    psackling/ids.hpp
    psackling/namespace.hpp
    psackling/context.hpp
    psackling/module_loader.hpp
    psackling/module_loader.cpp
    psackling/pipeline.hpp
    psackling/pipeline.cpp
    psackling/llvm_lowering_context.hpp
    psackling/llvm_lowering_context.cpp
    psackling/expr_lowering.hpp
    psackling/expr_lowering.cpp
    psackling/const_eval.hpp
    psackling/const_eval.cpp
    psackling/char_literal.hpp
    psackling/type_name_extractor.hpp
    psackling/type_name_extractor.cpp
)

llvm_map_components_to_libnames(psackling_llvm_libs
    Core
    MC
    Passes
    Support
    nativecodegen
)

target_include_directories(libpsackling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(libpsackling PRIVATE ${CMAKE_SOURCE_DIR}/common/system_command/src)
target_include_directories(libpsackling PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(libpsackling PUBLIC ${psackling_llvm_libs})
target_link_libraries(libpsackling PRIVATE Base Hashing Logger psac PstParser SystemCommand llvm_helpers)
target_compile_options(libpsackling PRIVATE ${PSAC_FLAGS})

add_example(psacklinger main.cpp USES libpsackling Base Init Clah Logger SystemCommand llvm_helpers)


add_example(psacklinger_test tests/psacklinger_test.cpp USES libpsackling psack_corpus Tester Base Init Clah Logger SystemCommand llvm_helpers)
