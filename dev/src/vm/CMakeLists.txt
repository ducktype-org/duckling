add_custom_test_pack(vm)

function(add_files)
  math(EXPR indices "${ARGC} - 1")

  foreach(index RANGE ${indices})
    set(tmp ${tmp} "${CMAKE_CURRENT_LIST_DIR}/${ARGV${index}}")
  endforeach()

  set(files ${tmp} PARENT_SCOPE)
endfunction()

# Add vm files
add_subdirectory(src/vm)

set(exec_sources
    src/vm/main.cpp
    src/vm/server.cpp
    src/vm/cli.cpp
    src/vm/vm_repl.cpp
    src/vm/debugger/vm_debug_cli.cpp
    src/vm/debugger/vm_debug_core.cpp
)

# There are three "libraries":.
# - LibCommonVM from ${vm_common_sources}
#    - this is created from the source files that do not require CONFIG compile definitions.
# - LibThreadVM_{sc/tc} from ${vm_config_dependent_sources}
#    - this is created from the source files that require CONFIG compile definitions.
# - MainVM from ${exec_sources}
#    - this are the main files that are not dependent on CONFIG compile definitions.
#
# The LibCommonVM and LibThreadVM_{sc/tc} should always be linked together
# as they depend on each other.

set(vm_dependencies
  Base Clah Crow::Crow Filesystem TPC JSON Init Logger
)

add_library(LibCommonVM OBJECT ${vm_common_sources})
make_module(LibCommonVM USES ${vm_dependencies})

add_library(MainVM OBJECT ${exec_sources})
make_module(MainVM USES ${vm_dependencies})

# Create the LibVM_{sc/tc} static libraries and the VM_{sc/tc} executables.
# In the middle it creates the LibThreadVM_{sc/tc} object libraries.
# The configuration depends on the suffix and the compile_def.
function(add_vm_variant suffix compile_def)
    set(lib_thread_vm_name "LibThreadVM_${suffix}")
    add_library(${lib_thread_vm_name} OBJECT ${vm_config_dependent_sources})
    make_module(${lib_thread_vm_name} USES ${vm_dependencies})

    if (compile_def)
        target_compile_definitions(${lib_thread_vm_name} PRIVATE "${compile_def}")
    endif()

    set(lib_vm_name "LibVM_${suffix}")
    add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}>)
    make_module(${lib_vm_name} USES ${vm_dependencies})

    set(exec_name "VM_${suffix}")
    add_executable(${exec_name} $<TARGET_OBJECTS:MainVM>)
    target_link_libraries(${exec_name} ${lib_vm_name})
endfunction()

# Define suffixes and their respective compile definitions
add_vm_variant(tc "USE_TAIL_CALLS")
add_vm_variant(sc "USE_SWITCH_CASE")

# Default alias to "sc"
add_library(LibVM  $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:LibThreadVM_sc>)
make_module(LibVM USES ${vm_dependencies})
add_executable(VM $<TARGET_OBJECTS:MainVM>)
target_link_libraries(VM LibVM)

add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)
