if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # JIT_ENABLED nie działa
    process_to_binary(tests/memory/data.c tests/jit_memory_test.cpp)
    duck_add_test(vm jit_memory tests/jit_memory_test.cpp USES)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    process_to_binary(stencils/wrapper.cpp copy-and-patch.cpp)
    add_files(copy-and-patch.cpp)
endif()

set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)
