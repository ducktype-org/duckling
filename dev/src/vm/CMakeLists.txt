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
  Base Clah Crow::Crow Filesystem TPC JSON Init Logger Diagnostic
)

set(vm_not_header_deps
  Base Clah Filesystem TPC Init Logger Diagnostic
)

set(vm_dep_objects "")
foreach(dep ${vm_not_header_deps})
    if(TARGET ${dep})
        list(APPEND vm_dep_objects "$<TARGET_OBJECTS:${dep}>")
    endif()
endforeach()

llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Support
    nativecodegen)

add_library(JitDep OBJECT
    ${jit_dependency}
)
make_module(JitDep USES llvm_helpers Base Diagnostic) # TODO update
target_include_directories(JitDep PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(JitDep ${llvm_libs})

add_library(LibCommonVM OBJECT ${vm_common_sources})
make_module(LibCommonVM USES ${vm_dependencies} ${JitDep})

add_library(MainVM OBJECT ${exec_sources})
make_module(MainVM USES ${vm_dependencies} ${JitDep})
target_link_libraries(MainVM PUBLIC ${JitDep})
target_include_directories(MainVM PRIVATE $<TARGET_PROPERTY:JitDep,INTERFACE_INCLUDE_DIRECTORIES>)


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


    set(common_bc ${CMAKE_CURRENT_BINARY_DIR}/"common_${suffix}.bc")

    add_custom_command(
        OUTPUT ${common_bc}
        COMMAND llvm-link-19
            $<TARGET_OBJECTS:LibCommonVM>
            $<TARGET_OBJECTS:${lib_thread_vm_name}>
            ${vm_dep_objects}
            -o ${common_bc}.tmp
        
        COMMAND opt-19
            -O3
            --enable-lto-internalization
            #--inline-threshold=20000 # mozna dobra stala
            --module-summary
            ${common_bc}.tmp
            -o ${common_bc}

        COMMAND_EXPAND_LISTS
        DEPENDS LibCommonVM ${lib_thread_vm_name} ${vm_dep_objects}
        COMMENT "Full O3 + LTO + Aggressive Inline for ${suffix}"
    )

    add_custom_target(CommonVMBitcode_${suffix} ALL
        DEPENDS ${common_bc} ${lib_thread_vm_name}
    )
    add_dependencies(JitDep CommonVMBitcode_${suffix})

    set(lib_vm_name "LibVM_${suffix}")
    add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}>)
    
    target_link_libraries(${lib_vm_name} ${llvm_libs})

    make_module(${lib_vm_name} USES ${vm_dependencies})
    
    target_compile_options(${lib_vm_name} PRIVATE
        -fembed-dir=${CMAKE_CURRENT_BINARY_DIR}
    )

    set(exec_name "VM_${suffix}")
    add_executable(${exec_name} $<TARGET_OBJECTS:MainVM> $<TARGET_OBJECTS:JitDep>)
    target_link_libraries(${exec_name} PRIVATE ${lib_vm_name})
    target_link_libraries(${lib_vm_name} ${llvm_libs})
endfunction()

# Define suffixes and their respective compile definitions
add_vm_variant(tc "USE_TAIL_CALLS")
add_vm_variant(sc "USE_SWITCH_CASE")

# Default alias to "sc"
add_library(LibVM  $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:LibThreadVM_sc>)
target_link_libraries(LibVM ${llvm_libs} ${JitDep})
make_module(LibVM USES ${vm_dependencies})
add_executable(VM $<TARGET_OBJECTS:MainVM> $<TARGET_OBJECTS:JitDep>)
target_link_libraries(VM LibVM ${llvm_libs} ${JitDep})



add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)
