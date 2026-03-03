function(process_to_binary source_file destination_file)
    get_filename_component(
        source_file_abs "${source_file}"
        ABSOLUTE BASE_DIR
        "${CMAKE_CURRENT_LIST_DIR}"
    )
    get_filename_component(
        destination_file_abs "${destination_file}"
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
    if(source_file_stem STREQUAL "wrapper")
        target_include_directories(${source_file_stem}-compiled PRIVATE
            "${CMAKE_SOURCE_DIR}/src/vm/src"
            "${CMAKE_SOURCE_DIR}/src/vm/src_private"
        )
        target_link_libraries(${source_file_stem}-compiled
            Base Clah Filesystem TPC Init Logger Diagnostic Crow::Crow JSON
        )
    endif()

    set(linked_out "${CMAKE_CURRENT_BINARY_DIR}/${source_file_stem}-linked")
    set(nm_out "${CMAKE_CURRENT_BINARY_DIR}/${source_file_stem}-nm")
    set(text_out "${CMAKE_CURRENT_BINARY_DIR}/${source_file_stem}-text")
    
    find_program(LLVM_NM NAMES llvm-nm-19 llvm-nm REQUIRED)
    find_program(LLVM_OBJCOPY NAMES llvm-objcopy-19 llvm-objcopy REQUIRED)

    set(flto "")
    if(ENABLE_LINK_TIME_OPTIMIZATION)
        set(flto "-flto")
    endif()

    add_custom_command(
        OUTPUT ${linked_out}
        COMMAND ${CMAKE_CXX_COMPILER} -r ${flto} -Bsymbolic-functions $<TARGET_OBJECTS:${source_file_stem}-compiled> -o ${linked_out}
        DEPENDS ${source_file_stem}-compiled
    )
    add_custom_command(
        OUTPUT ${nm_out}
        COMMAND
            sh -c
            "\"${LLVM_NM}\" -S --format=posix \"${linked_out}\" | awk 'NF >= 4 { print \"{ .name =\\\"\" $1 \"\\\", .type = \\\"\" $2 \"\\\", .place = 0x\" $3 \", .size = 0x\" $4 \"},\" }' > \"${nm_out}\""
        DEPENDS ${linked_out}
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${text_out}
        COMMAND ${LLVM_OBJCOPY} -O binary --only-section=.text ${linked_out} ${text_out} 
        DEPENDS ${linked_out}
    )

    add_custom_target(${source_file_stem}-binary-artifacts
        DEPENDS "${nm_out}" "${text_out}"
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

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set_property(
            SOURCE "${destination_file_abs}"
            DIRECTORY ${destination_scopes}
            APPEND
            PROPERTY COMPILE_OPTIONS
            "--embed-dir=${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()
    
    set_property(
        SOURCE "${destination_file_abs}"
        DIRECTORY ${destination_scopes}
        APPEND
        PROPERTY OBJECT_DEPENDS
        "${nm_out}" "${text_out}"
    )
endfunction()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") # JIT_ENABLED nie działa
    process_to_binary(tests/memory/data.c tests/jit_memory_test.cpp)
    duck_add_test(vm jit_memory tests/jit_memory_test.cpp USES)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    process_to_binary(stencils/wrapper.cpp copy-and-patch.cpp)
    add_files(copy-and-patch.cpp)
endif()

set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)
