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
  Base Clah Crow::Crow Filesystem TPC JSON Init Logger llvm_helpers
)

llvm_map_components_to_libnames(llvm_libs
    Core
    ExecutionEngine
    Interpreter
    MC
    MCJIT
    Support
    nativecodegen)

add_library(LibCommonVM OBJECT ${vm_common_sources})
target_include_directories(LibCommonVM PRIVATE ${LLVM_INCLUDE_DIRS})
make_module(LibCommonVM USES ${vm_dependencies})
target_link_libraries(LibCommonVM ${llvm_libs})

add_library(MainVM OBJECT ${exec_sources})
make_module(MainVM USES ${vm_dependencies})

# Prepares the bitcode implementation of opcodes for JIT.
function(prepare_JIT_opcodes suffix lib_thread_vm_name compile_def)
    set(opcodes_exec_name "ExecOpcodesDebug${suffix}")
    add_library(${opcodes_exec_name} OBJECT
        src/vm/core/thread/opcode_functions/opcodes_functions_impl_exec.cpp
    )
    target_include_directories(${opcodes_exec_name} PRIVATE 
        $<TARGET_PROPERTY:${lib_thread_vm_name},INCLUDE_DIRECTORIES>
    )
    target_compile_definitions(${opcodes_exec_name} PRIVATE 
        $<TARGET_PROPERTY:${lib_thread_vm_name},COMPILE_DEFINITIONS>
        "${compile_def}"
    )
    target_compile_options(${opcodes_exec_name} PRIVATE -flto -fPIC -fomit-frame-pointer -O3 -g0 -emit-llvm)
    find_program(LLVM_LINK_PATH "llvm-link-19" REQUIRED)

    set(bitcode_exec "${CMAKE_CURRENT_BINARY_DIR}/opcodes_exec_${suffix}.bc")

    add_custom_command(
        OUTPUT ${bitcode_exec}
        COMMAND ${LLVM_LINK_PATH} 
                    $<TARGET_OBJECTS:${opcodes_exec_name}> 
                    -o ${bitcode_exec}
        DEPENDS ${opcodes_exec_name}
        COMMENT "Linking optimized exec bitcode: ${bitcode_exec}"
    )

    add_custom_target(GenerateJitObject_${suffix} ALL DEPENDS ${bitcode_exec})
endfunction()

# Create the LibVM_{sc/tc} static libraries and the VM_{sc/tc} executables.
# In the middle it creates the LibThreadVM_{sc/tc} object libraries.
# The configuration depends on the suffix and the compile_def.
function(add_vm_variant suffix compile_def)
    set(lib_thread_vm_name "LibThreadVM_${suffix}")
    add_library(${lib_thread_vm_name} OBJECT ${vm_config_dependent_sources})
    make_module(${lib_thread_vm_name} USES ${vm_dependencies})

    target_include_directories(${lib_thread_vm_name} PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_libraries(${lib_thread_vm_name} ${llvm_libs})

    if (compile_def)
        target_compile_definitions(${lib_thread_vm_name} PRIVATE "${compile_def}")
    endif()

    prepare_JIT_opcodes(${suffix} ${lib_thread_vm_name} "${compile_def}")

    set_source_files_properties(${bitcode_exec} PROPERTIES GENERATED TRUE)

    target_sources(LibCommonVM PRIVATE ${bitcode_exec})

    set(lib_vm_name "LibVM_${suffix}")
    add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}>)
    target_link_libraries(${lib_vm_name} ${llvm_libs})

    target_compile_options(${lib_vm_name} PRIVATE
        -fembed-dir=${CMAKE_CURRENT_BINARY_DIR}
    )

    make_module(${lib_vm_name} USES ${vm_dependencies} )
    
    set(exec_name "VM_${suffix}")
    add_executable(${exec_name} $<TARGET_OBJECTS:MainVM>)
    target_link_libraries(${exec_name} PRIVATE ${lib_vm_name})
    target_link_libraries(${lib_vm_name} ${llvm_libs})

    add_dependencies(${exec_name} GenerateJitObject_${suffix})

endfunction()

# Define suffixes and their respective compile definitions
add_vm_variant(tc "USE_TAIL_CALLS")
add_vm_variant(sc "USE_SWITCH_CASE")

# Default alias to "sc"
add_library(LibVM  $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:LibThreadVM_sc>)
target_link_libraries(LibVM ${llvm_libs})
make_module(LibVM USES ${vm_dependencies})
add_executable(VM $<TARGET_OBJECTS:MainVM>)
target_link_libraries(VM LibVM ${llvm_libs})

add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)
