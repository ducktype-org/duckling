include(CMakePrintHelpers)

function(process_to_binary source_file)
    get_filename_component(
        source_file_abs "${source_file}"
        ABSOLUTE BASE_DIR
        "${CMAKE_CURRENT_LIST_DIR}"
    )

    if(NOT EXISTS "${source_file_abs}")
        message(
            FATAL_ERROR 
            "process_to_binary source file not found: ${source_file_abs}"
        )
    endif()

    get_filename_component(
        source_file_stem "${source_file_abs}"
        NAME_WE
    )
    
    
    add_library(${source_file_stem}-compiled OBJECT ${source_file})
    target_compile_options(${source_file_stem}-compiled PRIVATE 
        -fPIC
        -fno-plt
        -fno-semantic-interposition
    )

    set(linked_out "${CMAKE_CURRENT_BINARY_DIR}/${source_file_stem}-linked")
    set(nm_out "${CMAKE_CURRENT_BINARY_DIR}/${source_file_stem}-nm")
    set(text_out "${CMAKE_CURRENT_BINARY_DIR}/${source_file_stem}-text")
    
    cmake_print_variables(linked_out nm_out text_out)
    
    find_program(LLVM_NM NAMES llvm-nm-19 llvm-nm REQUIRED)
    find_program(LLVM_OBJCOPY NAMES llvm-objcopy-19 llvm-objcopy REQUIRED)


    add_custom_command(
        OUTPUT ${linked_out}
        COMMAND ${CMAKE_LINKER} -r -Bsymbolic-functions $<TARGET_OBJECTS:${source_file_stem} -o ${linked_out}
        DEPENDS ${source_file_stem}-compiled
    )
    add_custom_command(
        OUTPUT ${nm_out}
        COMMAND nmer ${LLVM_NM}
        DEPENDS ${source_file_stem}-compiled
    )
    add_custom_command(
        OUTPUT ${text_out}
        COMMAND ${LLVM_OBJCOPY} -O binary --only-section=.text ${linked_out} ${text_out} 
        DEPENDS ${linked_out}
    )
    set(jit_text_out "${text_out}" PARENT_SCOPE)
    set(jit_nm_out   "${nm_out}"   PARENT_SCOPE)
endfunction()

add_subdirectory(copy-and-patch)
