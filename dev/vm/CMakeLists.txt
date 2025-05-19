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
)

# There are two "libraries" and one executable that should always be linked toghethers.
# - LibCommonVM from ${vm_common_sources}
#    - this is created from the source files that do not require CONFIG compile definitions.
# - LibThreadVM_{sc/cg/tc} from ${vm_config_dependent_sources}
#    - this is created from the source files that require CONFIG compile definitions.
# - MainVM from ${exec_sources}
#    - this is the main executable that uses LibCommonVM and LibThreadVM_{sc/cg/tc}.

add_library(LibCommonVM OBJECT ${vm_common_sources})
make_module(LibCommonVM USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

add_library(MainVM OBJECT ${exec_sources})
make_module(MainVM USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

function(add_vm_variant suffix compile_def)
    set(lib_thread_vm_name "LibThreadVM_${suffix}")
    add_library(${lib_thread_vm_name} OBJECT ${vm_config_dependent_sources})
    make_module(${lib_thread_vm_name} USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

    if (compile_def)
        target_compile_definitions(${lib_thread_vm_name} PRIVATE "${compile_def}")
    endif()


    set(lib_vm_name "LibVM_${suffix}")
    add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}>)
    make_module(${lib_vm_name} USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

    set(exec_name "VM_${suffix}")
    add_executable(${exec_name} $<TARGET_OBJECTS:MainVM>)
    target_link_libraries(${exec_name} ${lib_vm_name})
endfunction()

# Define suffixes and their respective compile definitions
add_vm_variant(tc "")
add_vm_variant(sc "USE_SWITCH_CASE")
add_vm_variant(cg "USE_COMPUTED_GOTO")

# Default alias to "tc"
add_library(LibVM ALIAS LibVM_tc)
add_executable(VM ALIAS VM_tc)

add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)
