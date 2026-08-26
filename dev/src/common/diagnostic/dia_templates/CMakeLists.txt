# @author Wojciech Rzepliński

# This is a CMake flag and not a macro in the C++ files to allow not to run the
# embedding from the CMake side at all. 
option(EMBED_ASSETS "Embed files into the binary for self-contained builds" OFF)

if(EMBED_ASSETS)
# Embeds YAML template files as binary data into the executable
# Generates a zero-copy std::string_view-based registry for runtime access
# There is another more portable way to do this with converting files into C++ arrays

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
set(EMBED_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/src/diagnostic/dia_templates")

# Lists to collect generated files
set(EMBEDDED_OBJECTS "")
set(EXTERN_DECLARATIONS "")
set(REGISTRY_ENTRIES "")

# Process each template file
foreach(TEMPLATE_FILE ${TEMPLATE_FILES})
    # Get the relative path and remove the file extension for the hashmap key
    set(TEMPLATE_KEY "${TEMPLATE_FILE}")
    string(REGEX REPLACE "\\.(yaml|yml)$" "" TEMPLATE_KEY "${TEMPLATE_KEY}")
    
    # Create a safe symbol name by replacing / and . with _
    string(REPLACE "/" "_" SYMBOL_NAME "${TEMPLATE_FILE}")
    string(REPLACE "." "_" SYMBOL_NAME "${SYMBOL_NAME}")
    
    # Full path to the template file
    set(TEMPLATE_PATH_RELATIVE "${TEMPLATE_FILE}")
    set(TEMPLATE_PATH_FULL "${CMAKE_CURRENT_SOURCE_DIR}/templates/${TEMPLATE_FILE}")
    
    # Output object file path
    set(OBJ_FILE "${EMBED_GEN_DIR}/${SYMBOL_NAME}.o")
    
    # Custom command to convert YAML to object file
    add_custom_command(
        OUTPUT ${OBJ_FILE}
        COMMAND ${OBJCOPY_TOOL} -I binary -O ${OBJCOPY_FORMAT}
                --rename-section .data=.rodata,alloc,load,readonly,data,contents
                "${TEMPLATE_PATH_RELATIVE}" "${OBJ_FILE}"
        DEPENDS "${TEMPLATE_PATH_FULL}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/templates"
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
add_library(DiaTemplates STATIC ${REGISTRY_CPP} ${EMBEDDED_OBJECTS})
add_dependencies(DiaTemplates embedded_templates_generate)
target_compile_definitions(DiaTemplates PUBLIC DIA_TEMPLATES_USE_EMBEDDED=1)
make_module(DiaTemplates USES)

else()

add_library(DiaTemplates STATIC
    src/diagnostic/dia_templates/filesystem_implementation.cpp
)
target_compile_definitions(DiaTemplates PRIVATE CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
make_module(DiaTemplates USES Base)

endif()
