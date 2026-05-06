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
    psackling/ids.hpp
    psackling/namespace.hpp
    psackling/context.hpp
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
target_link_libraries(libpsackling PRIVATE Base Logger psac PstParser SystemCommand llvm_helpers)
target_compile_options(libpsackling PRIVATE ${PSAC_FLAGS})

add_example(psacklinger main.cpp USES libpsackling Base Init Clah Logger SystemCommand llvm_helpers)


add_example(psacklinger_test tests/test.cpp USES libpsackling Tester Base Init Clah Logger SystemCommand llvm_helpers)
