# @author Wojciech Rzepliński

# This is a CMake flag and not a macro in the C++ files to allow not to run the
# embedding from the CMake side at all. 
option(EMBED_ASSETS "Embed files into the binary for self-contained builds" OFF)

if(EMBED_ASSETS)
# Embeds the YAML template files into the executable, as C++ byte arrays generated here, with a
# zero-copy std::string_view-based registry for runtime access.
# Portable: unlike `objcopy -I binary` (ELF only), it works for Mach-O (macOS) and any compiler.

# Find all YAML template files. CONFIGURE_DEPENDS: a template added (or removed) later must
# re-run CMake, otherwise an existing build dir silently leaves it out.
file(GLOB_RECURSE TEMPLATE_FILES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/templates"
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/templates/*.yaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/templates/*.yml"
)
list(SORT TEMPLATE_FILES)

list(LENGTH TEMPLATE_FILES LIST_LENGTH)
message(STATUS "Found ${LIST_LENGTH} template files to embed")

# Directory for generated file
set(EMBED_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/src/diagnostic/dia_templates")

# Collected parts of the generated file
set(EMBEDDED_DATA "")
set(REGISTRY_ENTRIES "")

# Process each template file
set(TEMPLATE_INDEX 0)
foreach(TEMPLATE_FILE ${TEMPLATE_FILES})
    # Get the relative path and remove the file extension for the hashmap key
    set(TEMPLATE_KEY "${TEMPLATE_FILE}")
    string(REGEX REPLACE "\\.(yaml|yml)$" "" TEMPLATE_KEY "${TEMPLATE_KEY}")

    # The array is named by the position in the sorted list (the path is in a comment above it):
    # always a valid identifier, and two paths can never map to the same name.
    set(SYMBOL_NAME "template_${TEMPLATE_INDEX}")
    math(EXPR TEMPLATE_INDEX "${TEMPLATE_INDEX} + 1")

    set(TEMPLATE_PATH_FULL "${CMAKE_CURRENT_SOURCE_DIR}/templates/${TEMPLATE_FILE}")
    # The file is read here, at configure time: re-configure when it changes.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${TEMPLATE_PATH_FULL}")

    # The content as `0x..,` bytes, followed by a terminating zero, so that an empty file still
    # gives a valid array (the zero is not part of the view). unsigned char: bytes above 0x7f
    # would be narrowing conversions for a (signed) char.
    file(READ "${TEMPLATE_PATH_FULL}" TEMPLATE_HEX HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," TEMPLATE_HEX "${TEMPLATE_HEX}")
    string(APPEND EMBEDDED_DATA "    // ${TEMPLATE_FILE}\n")
    string(APPEND EMBEDDED_DATA "    constexpr unsigned char ${SYMBOL_NAME}[] = {${TEMPLATE_HEX}0x00};\n")

    # Generate registry entry
    string(APPEND REGISTRY_ENTRIES "    {\n")
    string(APPEND REGISTRY_ENTRIES "        \"${TEMPLATE_KEY}\",\n")
    string(APPEND REGISTRY_ENTRIES "        std::string_view(\n")
    string(APPEND REGISTRY_ENTRIES "            reinterpret_cast<const char*>(${SYMBOL_NAME}),\n")
    string(APPEND REGISTRY_ENTRIES "            sizeof(${SYMBOL_NAME}) - 1\n")
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

# A library with the registry and the embedded data
add_library(DiaTemplates STATIC ${REGISTRY_CPP})
target_compile_definitions(DiaTemplates PUBLIC DIA_TEMPLATES_USE_EMBEDDED=1)
make_module(DiaTemplates USES)

else()

add_library(DiaTemplates STATIC
    src/diagnostic/dia_templates/filesystem_implementation.cpp
)
target_compile_definitions(DiaTemplates PRIVATE CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
make_module(DiaTemplates USES Base)

endif()
