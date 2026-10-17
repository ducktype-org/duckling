add_library(pq_shared
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

target_include_directories(pq_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(pq_shared PRIVATE ${CMAKE_SOURCE_DIR}/common/system_command/src)
target_include_directories(pq_shared PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(pq_shared PUBLIC ${psackling_llvm_libs})
target_link_libraries(pq_shared PRIVATE Base Hashing Logger psac PstParser SystemCommand llvm_helpers)
target_compile_options(pq_shared PRIVATE ${PSAC_FLAGS})

