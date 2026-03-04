function(process_to_binary binary_file destination_file binary_artifact_prefix)
    set(extra_deps ${ARGN})
    get_filename_component(
        destination_file_abs "${destination_file}"
        ABSOLUTE BASE_DIR
        "${CMAKE_CURRENT_LIST_DIR}"
    )

    set(linked_out "${CMAKE_CURRENT_BINARY_DIR}/${binary_artifact_prefix}-linked")
    set(nm_out "${CMAKE_CURRENT_BINARY_DIR}/${binary_artifact_prefix}-nm")
    set(text_out "${CMAKE_CURRENT_BINARY_DIR}/${binary_artifact_prefix}-text")

    find_program(LLVM_NM NAMES llvm-nm-19 llvm-nm REQUIRED)
    find_program(LLVM_OBJCOPY NAMES llvm-objcopy-19 llvm-objcopy REQUIRED)


    add_custom_command(
        OUTPUT ${linked_out}
        COMMAND ${CMAKE_CXX_COMPILER} -r ${flto} -Bsymbolic-functions ${binary_file} -o ${linked_out}
        DEPENDS ${binary_file} ${extra_deps}
    )
    add_custom_command(
        OUTPUT ${nm_out}
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/analyze-bin/analyze-bin.py" "${linked_out}" "--output=${nm_out}"
        DEPENDS ${linked_out} ${CMAKE_CURRENT_SOURCE_DIR}/analyze-bin/analyze-bin.py
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${text_out}
        COMMAND ${LLVM_OBJCOPY} -O binary --only-section=.text ${linked_out} ${text_out}
        DEPENDS ${linked_out}
    )
    set_source_files_properties("${nm_out}" PROPERTIES GENERATED TRUE)
    set_source_files_properties("${text_out}" PROPERTIES GENERATED TRUE)

    set(destination_scopes
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src/vm"
    )

    set_property(
        SOURCE "${destination_file_abs}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

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
        "${nm_out}" "${text_out}"
    )
endfunction()

if(JIT_ENABLED)
    add_library(mock_stencils OBJECT tests/mock_stencils.c)
    target_compile_options(mock_stencils PRIVATE
        -fPIC
        -fno-plt
        -fno-semantic-interposition
    )
    process_to_binary("$<TARGET_OBJECTS:mock_stencils>" tests/jit_memory_test.cpp "mock_stencils" mock_stencils)
    duck_add_test(vm jit_memory tests/jit_memory_test.cpp USES)


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
            -O3
            $<TARGET_OBJECTS:cp-stencils_bc>
            -o
            ${cp_stencils}
        COMMAND_EXPAND_LISTS
        DEPENDS cp-stencils_bc
     )
        
    process_to_binary("${cp_stencils}" copy-and-patch.cpp "wrapper")
    add_custom_target(
        wrapper-binary-artifacts-target
        DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/wrapper-nm
            ${CMAKE_CURRENT_BINARY_DIR}/wrapper-text
    )
    add_files(copy-and-patch.cpp)

    set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)

endif()
