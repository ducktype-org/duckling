add_subdirectory(llvm/)

# Host tool exporting opcode metadata (opcodes.json: opcode values + non-jittable list) to the
# python pipeline, so that micro_instruction_definitions.def.hpp / non_jittable.def.hpp stay the
# single source of truth shared between C++ and python.
add_executable(print_opcodes copy-and-patch/print_opcodes.cpp)

set(opcodes_json ${CMAKE_CURRENT_BINARY_DIR}/opcodes.json)
add_custom_command(
    OUTPUT ${opcodes_json}
    COMMAND print_opcodes >${opcodes_json}
    DEPENDS print_opcodes
)
add_custom_target(PrintOpcodesJson DEPENDS ${opcodes_json})
# Consumed by the jittable_interface step in src/vm/CMakeLists.txt.
set_property(GLOBAL PROPERTY OPCODES_JSON_PATH ${opcodes_json})

if (CNP_ENABLED)
    add_subdirectory(copy-and-patch/)
endif()

add_files(
    jit_helper.cpp
)
set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)
set(vm_common_sources ${vm_common_sources} PARENT_SCOPE)
set(jit_dependency ${jit_dependency} PARENT_SCOPE)
