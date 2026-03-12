# Analyze and process the `binary_file`, preparing to embed in `destination_file`
function(process_to_binary binary_file destination_file binary_artifact_prefix)
    set(extra_deps ${ARGN})
    get_filename_component(
        destination_file_abs "${destination_file}"
        ABSOLUTE BASE_DIR
        "${CMAKE_CURRENT_LIST_DIR}"
    )

    set(linked_out "${CMAKE_CURRENT_BINARY_DIR}/${binary_artifact_prefix}-so")
    set(nm_out "${CMAKE_CURRENT_BINARY_DIR}/${binary_artifact_prefix}-nm")

    find_program(LLVM_NM NAMES llvm-nm-19 llvm-nm REQUIRED)
    find_program(LLVM_OBJCOPY NAMES llvm-objcopy-19 llvm-objcopy REQUIRED)

    # Force resolution of local function calls (is it needed?)
    add_custom_command(
        OUTPUT ${linked_out}
        COMMAND ${CMAKE_CXX_COMPILER}
        -Bsymbolic-functions
        -fno-plt
        -shared
        -fPIC
        ${binary_file}
        -o ${linked_out}
        DEPENDS ${binary_file} ${extra_deps}
    )
    set_source_files_properties("${linked_out}" PROPERTIES GENERATED TRUE)

    # Analyze and generate data from the binary
    add_custom_command(
        OUTPUT ${nm_out}
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/analyze-bin/analyze-bin.py" "${linked_out}" "--output=${nm_out}"
        DEPENDS ${linked_out} ${CMAKE_CURRENT_SOURCE_DIR}/analyze-bin/analyze-bin.py
        VERBATIM
    )

    set_source_files_properties("${linked_out}" "${nm_out}"
        PROPERTIES GENERATED TRUE
        HEADER_FILE_ONLY TRUE
    )

    set(destination_scopes
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src/vm"
    )

    # Include data generated from the binary
    set_property(
        SOURCE "${destination_file_abs}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

    # Embed the binary
    set_property(
        SOURCE "${destination_file_abs}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY COMPILE_OPTIONS
        "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}"
    )

    set_property(
        SOURCE "${destination_file_abs}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY OBJECT_DEPENDS
        "${nm_out}" "${linked_out}"
    )
endfunction()

add_library(mock_stencils OBJECT tests/mock_stencils.c)
target_compile_options(mock_stencils PRIVATE
    -fPIC
    -fno-plt
    -fno-semantic-interposition
    -mllvm=--inline-threshold=3000000
)
process_to_binary("$<TARGET_OBJECTS:mock_stencils>" tests/jit_memory_test.cpp "mock_stencils" mock_stencils)
duck_add_test(vm jit_memory tests/jit_memory_test.cpp USES)

# prepare to include it in the full LTO of opfuncs
add_library(cp-stencils_bc OBJECT stencils/wrapper.cpp)
target_compile_options(cp-stencils_bc PRIVATE -emit-llvm)
target_link_libraries(cp-stencils_bc PRIVATE LibCommonVM)

if(NOT JIT_LLVM_LLC)
    find_program(JIT_LLVM_LLC NAMES llc-19 llc REQUIRED)
endif()

set(cp_stencils "${CMAKE_CURRENT_BINARY_DIR}/wrapper")

# compile to binary
add_custom_command(
    OUTPUT ${cp_stencils}
    COMMAND ${JIT_LLVM_LLC}
    -filetype=obj
    --relocation-model=pic
    -O3
    $<TARGET_PROPERTY:CommonVMBitcode_sc,VM_COMMON_BC>
    -o
    ${cp_stencils}
    DEPENDS CommonVMBitcode_sc
)

process_to_binary("${cp_stencils}" copy-and-patch.cpp "wrapper")
set(wrapper_binary_artifacts
    "${CMAKE_CURRENT_BINARY_DIR}/wrapper-nm"
    "${CMAKE_CURRENT_BINARY_DIR}/wrapper-so"
)
add_custom_target(
    cp-stencils-target
    DEPENDS
    ${wrapper_binary_artifacts}
)
set(jit_dependency
    ${jit_dependency}
    "${CMAKE_CURRENT_LIST_DIR}/memory/memory.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/copy-and-patch.cpp"
    ${wrapper_binary_artifacts}
    PARENT_SCOPE
)
