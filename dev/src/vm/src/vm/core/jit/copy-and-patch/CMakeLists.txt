file(GLOB analyze_deps "${CMAKE_SOURCE_DIR}/scripts/py/jit/*.py")

# Analyze and process the `binary_file`, preparing to embed in `destination_file`
function(process_to_binary binary_file destination_file binary_artifact_prefix analyze_arg)
    set(extra_deps ${ARGN})
    get_filename_component(
        destination_file_abs "${destination_file}"
        ABSOLUTE BASE_DIR
        "${CMAKE_CURRENT_LIST_DIR}"
    )

    set(linked_out "${CMAKE_CURRENT_BINARY_DIR}/${binary_artifact_prefix}-so")
    set(nm_out "${CMAKE_CURRENT_BINARY_DIR}/${binary_artifact_prefix}-nm")

# The point here is to force out any relative dependencies between the functions,
# as it would interfere with template jit-ing. This involves shrinking PLT 
# as much as possible (as plt stubs are functions). All of those options 
# (even though unreasonable) seem to be necessary to avoid a linker error.
# @TODO: #2296 This takes way too long, ccache/make targets to simplify.
    add_custom_command(
        OUTPUT ${linked_out}
        COMMAND ${CMAKE_C_COMPILER}
        -O3
        -fno-plt
        -fPIC
        -fno-pie
        -ffunction-sections
        -mcmodel=large
        -c
        ${binary_file}
        -o ${linked_out}-1
        COMMAND ${CMAKE_C_COMPILER}
        -fPIC
        -shared
        ${linked_out}-1
        -o ${linked_out}
        DEPENDS ${binary_file} ${extra_deps}
    )
    set_source_files_properties("${linked_out}" PROPERTIES GENERATED TRUE)

    # Analyze and generate data from the binary
    add_custom_command(
        OUTPUT ${nm_out}
        COMMAND /usr/bin/env python3
        "${CMAKE_SOURCE_DIR}/scripts/py/jit/analyze_bin.py"
        "${linked_out}"
        "--output=${nm_out}"
        ${analyze_arg}
        "--shared"
        ${LLVM_TOOLS_ARGS}
        DEPENDS ${linked_out} ${analyze_deps}
    )

    set_source_files_properties("${linked_out}" "${nm_out}"
        PROPERTIES GENERATED TRUE
        HEADER_FILE_ONLY TRUE
    )

    set(destination_scopes
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src/vm"
    )

    set(artifacts "stencil-artifacts-${binary_artifact_prefix}")

    add_custom_target(${artifacts} DEPENDS "${nm_out}" "${linked_out}")

    # Include data generated from the binary
    set_property(
        SOURCE "${destination_file}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

    # Embed the binary
    set_property(
        SOURCE "${destination_file}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY COMPILE_OPTIONS
        "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_property(
        SOURCE "${destination_file}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY OBJECT_DEPENDS
        ${artifacts}
    )
endfunction()

process_to_binary(
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/mock_stencils.c 
    tests/jit_memory_test.cpp 
    "mock_stencils"
    "--accept-all-sections"
)

add_library(Stencils OBJECT memory/memory.cpp stencils/dynamic_library.cpp)
make_module(Stencils USES Base)

duck_add_test(vm jit_memory tests/jit_memory_test.cpp USES Base Stencils Tester)

# prepare to include it in the full LTO of opfuncs
add_library(cp-stencils_bc OBJECT stencils/stencils.cpp)
target_compile_options(cp-stencils_bc PRIVATE -emit-llvm)
target_link_libraries(cp-stencils_bc PRIVATE LibCommonVM)

process_to_binary(
    $<TARGET_PROPERTY:CommonVMBitcode_sc,VM_COMMON_BC> 
    stencil_holder.cpp 
    "stencils"
    ""
    $<TARGET_PROPERTY:CommonVMBitcode_sc,VM_COMMON_BC>)

add_files(
    memory/memory.cpp
    stencils/dynamic_library.cpp
    copy-and-patch.cpp
)
set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)

add_files(
    stencil_holder.cpp
)
set(jit_dependency ${jit_dependency} ${files} PARENT_SCOPE)
