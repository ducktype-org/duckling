add_library(pq_shared
    pq_shared/compiler.hpp
    pq_shared/compiler.cpp
    pq_shared/llvm.hpp
    pq_shared/llvm.cpp
    pq_shared/type.hpp
    pq_shared/symbol.hpp
    pq_shared/global_pass.hpp
    pq_shared/global_pass.cpp
    pq_shared/function_pass.hpp
    pq_shared/function_pass.cpp
    pq_shared/ids.hpp
    pq_shared/namespace.hpp
    pq_shared/context.hpp
    pq_shared/module_loader.hpp
    pq_shared/module_loader.cpp
    pq_shared/pipeline.hpp
    pq_shared/pipeline.cpp
    pq_shared/llvm_lowering_context.hpp
    pq_shared/llvm_lowering_context.cpp
    pq_shared/expr_lowering.hpp
    pq_shared/expr_lowering.cpp
    pq_shared/const_eval.hpp
    pq_shared/const_eval.cpp
    pq_shared/char_literal.hpp
    pq_shared/type_name_extractor.hpp
    pq_shared/type_name_extractor.cpp
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

