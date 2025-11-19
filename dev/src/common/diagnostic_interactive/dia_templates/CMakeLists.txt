# Embeds YAML template files as binary data into the executable
# Generates a zero-copy std::string_view-based registry for runtime access

# Detect platform and set appropriate objcopy tool and format
if(WIN32)
    # On Windows, use llvm-objcopy with COFF format
    find_program(OBJCOPY_TOOL llvm-objcopy)
    if(NOT OBJCOPY_TOOL)
        message(FATAL_ERROR "llvm-objcopy not found. Please install LLVM or ensure it's in PATH")
    endif()
    set(OBJCOPY_FORMAT "coff-x86-64")
else()
    # On Linux/Unix, use standard objcopy with ELF format
    find_program(OBJCOPY_TOOL objcopy)
    if(NOT OBJCOPY_TOOL)
        message(FATAL_ERROR "objcopy not found. Please install binutils")
    endif()
    set(OBJCOPY_FORMAT "elf64-x86-64")
endif()

# Find all YAML template files
file(GLOB_RECURSE TEMPLATE_FILES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/templates"
    "${CMAKE_CURRENT_SOURCE_DIR}/templates/*.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/templates/*.yml"
)

list(LENGTH TEMPLATE_FILES LIST_LENGTH)
message(STATUS "Found ${LIST_LENGTH} template files to embed")

# Directory for generated file
set(EMBED_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/src/diagnostic_interactive/dia_templates")

# Lists to collect generated files
set(EMBEDDED_OBJECTS "")
set(EXTERN_DECLARATIONS "")
set(REGISTRY_ENTRIES "")

# Process each template file
foreach(TEMPLATE_FILE ${TEMPLATE_FILES})
    # Get the relative path (this becomes the key in the map)
    set(TEMPLATE_KEY "${TEMPLATE_FILE}")
    
    # Create a safe symbol name by replacing / and . with _
    string(REPLACE "/" "_" SYMBOL_NAME "${TEMPLATE_FILE}")
    string(REPLACE "." "_" SYMBOL_NAME "${SYMBOL_NAME}")
    
    # Full path to the template file
    set(TEMPLATE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/templates/${TEMPLATE_FILE}")
    
    # Output object file path
    set(OBJ_FILE "${EMBED_GEN_DIR}/${SYMBOL_NAME}.o")
    
    # Custom command to convert YAML to object file
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND ${OBJCOPY_TOOL} -I binary -O ${OBJCOPY_FORMAT}
                --rename-section .data=.rodata,alloc,load,readonly,data,contents
                "${TEMPLATE_PATH}" "${OBJ_FILE}"
        DEPENDS "${TEMPLATE_PATH}"
        COMMENT "Embedding template: ${TEMPLATE_KEY}"
        VERBATIM
    )
    
    list(APPEND EMBEDDED_OBJECTS ${OBJ_FILE})
    
    # Generate extern declarations
    # Symbol names: _binary_<name>_start and _binary_<name>_end
    set(START_SYMBOL "_binary_${SYMBOL_NAME}_start")
    set(END_SYMBOL "_binary_${SYMBOL_NAME}_end")
    
    string(APPEND EXTERN_DECLARATIONS "    extern const char ${START_SYMBOL}[];\n")
    string(APPEND EXTERN_DECLARATIONS "    extern const char ${END_SYMBOL}[];\n")
    
    # Generate registry entry
    string(APPEND REGISTRY_ENTRIES "    {\n")
    string(APPEND REGISTRY_ENTRIES "        \"${TEMPLATE_KEY}\",\n")
    string(APPEND REGISTRY_ENTRIES "        std::string_view(\n")
    string(APPEND REGISTRY_ENTRIES "            reinterpret_cast<const char*>(${START_SYMBOL}),\n")
    string(APPEND REGISTRY_ENTRIES "            static_cast<size_t>(${END_SYMBOL} - ${START_SYMBOL})\n")
    string(APPEND REGISTRY_ENTRIES "        )\n")
    string(APPEND REGISTRY_ENTRIES "    },\n")
endforeach()

# Generate the registry C++ file
set(REGISTRY_CPP "${EMBED_GEN_DIR}/templates.cpp")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/templates.cpp.in"
    "${REGISTRY_CPP}"
    @ONLY
)

message(STATUS "Templates embedded cpp file generated")

# Create a custom target that depends on all embedded objects
add_custom_target(embedded_templates_generate
    DEPENDS ${EMBEDDED_OBJECTS}
)

# Create an object library for the registry
add_library(DiaTemplates OBJECT ${REGISTRY_CPP})
add_dependencies(DiaTemplates embedded_templates_generate)
make_module(DiaTemplates USES)

