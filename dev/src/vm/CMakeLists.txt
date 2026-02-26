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

if (JIT_ENABLED)

    set(vm_not_header_deps
        Base Clah Filesystem TPC Init Logger Diagnostic
    )

    set(vm_dep_objects_bc "")
    foreach(dep ${vm_not_header_deps})
        if(TARGET ${dep})
            add_library(${dep}_bc OBJECT $<TARGET_OBJECTS:${dep}>)
            target_compile_options(${dep}_bc PRIVATE -emit-llvm)
            list(APPEND vm_dep_objects_bc $<TARGET_OBJECTS:${dep}_bc>)
        endif()
    endforeach()


    llvm_map_components_to_libnames(llvm_libs
        Core
        ExecutionEngine
        Interpreter
        OrcJIT
        OrcShared
        OrcTargetProcess
        MC
        Support
        CodeGen
        AsmParser
        AsmPrinter
        X86AsmParser
        Object
        nativecodegen)

    # set(LLVM_BUILD_LLVM_DYLIB ON)
    # set(LLVM_LINK_LLVM_DYLIB ON)

    add_library(JitDep OBJECT
        ${jit_dependency}
    )
    make_module(JitDep USES llvm_helpers Base Diagnostic) # This list should be updated
    target_include_directories(JitDep PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_libraries(JitDep ${llvm_libs})

    add_library(LibCommonVM OBJECT ${vm_common_sources})
    make_module(LibCommonVM USES ${vm_dependencies} ${JitDep})

    add_library(MainVM OBJECT ${exec_sources})
    make_module(MainVM USES ${vm_dependencies} ${JitDep})
    target_link_libraries(MainVM PUBLIC ${JitDep})
    target_include_directories(MainVM PRIVATE $<TARGET_PROPERTY:JitDep,INTERFACE_INCLUDE_DIRECTORIES>)

    #create llvm files for sources of opcodes, to link them
    add_library(LibCommonVM_bc OBJECT ${vm_common_sources})
    make_module(LibCommonVM_bc USES ${vm_dependencies} ${JitDep})
    target_compile_options(LibCommonVM_bc PRIVATE -emit-llvm)

else()
    add_library(LibCommonVM OBJECT ${vm_common_sources})
    make_module(LibCommonVM USES ${vm_dependencies})

    add_library(MainVM OBJECT ${exec_sources})
    make_module(MainVM USES ${vm_dependencies})
endif()

# Create the LibVM_{sc/tc} static libraries and the VM_{sc/tc} executables.
# In the middle it creates the LibThreadVM_{sc/tc} object libraries.
# The configuration depends on the suffix and the compile_def.
function(add_vm_variant suffix compile_def)
    set(lib_thread_vm_name "LibThreadVM_${suffix}")
    add_library(${lib_thread_vm_name} OBJECT ${vm_config_dependent_sources})
    make_module(${lib_thread_vm_name} USES llvm_helpers ${vm_dependencies})
    target_include_directories(${lib_thread_vm_name} PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_libraries(${lib_thread_vm_name} ${llvm_libs})
    target_include_directories(${lib_thread_vm_name} PRIVATE ${LLVM_INCLUDE_DIRS})

    target_compile_definitions(${lib_thread_vm_name} PRIVATE "${compile_def}")

    if (JIT_ENABLED)
        target_link_libraries(${lib_thread_vm_name} ${llvm_libs})

        add_library(${lib_thread_vm_name}_bc OBJECT ${vm_config_dependent_sources})
        make_module(${lib_thread_vm_name}_bc USES llvm_helpers ${vm_dependencies})

        target_include_directories(${lib_thread_vm_name}_bc PRIVATE ${LLVM_INCLUDE_DIRS})
        target_link_libraries(${lib_thread_vm_name}_bc ${llvm_libs})
        
        target_compile_options(${lib_thread_vm_name}_bc PRIVATE -emit-llvm)
        target_compile_definitions(${lib_thread_vm_name}_bc PRIVATE "${compile_def}")

        set(common_bc ${CMAKE_CURRENT_BINARY_DIR}/common_${suffix}.bc)
        
        add_custom_command(
            OUTPUT ${common_bc}
            COMMAND ${JIT_LLVM_LINKER}
                $<TARGET_OBJECTS:LibCommonVM_bc>
                $<TARGET_OBJECTS:${lib_thread_vm_name}_bc>
                ${vm_dep_objects_bc}
                -o ${common_bc}.tmp
            COMMAND ${JIT_LLVM_OPT}
                -O3
                --enable-lto-internalization
                --inline-threshold=20000 # full inline
                --module-summary
                ${common_bc}.tmp
                -o ${common_bc}
            COMMAND_EXPAND_LISTS
            BYPRODUCTS ${common_bc}.tmp
            DEPENDS LibCommonVM_bc ${lib_thread_vm_name}_bc ${vm_dep_objects_bc}
            COMMENT "Full O3 + LTO + Aggressive Inline for ${suffix}"
        )

        add_custom_target(CommonVMBitcode_${suffix} DEPENDS ${common_bc})
        add_dependencies(JitDep CommonVMBitcode_${suffix})

        set(lib_vm_name "LibVM_${suffix}")
        add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}> $<TARGET_OBJECTS:JitDep>)
        
        target_link_libraries(${lib_vm_name} ${llvm_libs})

        make_module(${lib_vm_name} USES llvm_helpers ${vm_dependencies})
        
        target_compile_options(${lib_vm_name} PRIVATE
            -fembed-dir=${CMAKE_CURRENT_BINARY_DIR}
        )

        set(exec_name "VM_${suffix}")
        add_executable(${exec_name} $<TARGET_OBJECTS:MainVM> $<TARGET_OBJECTS:JitDep>)
        target_link_libraries(${exec_name} PRIVATE ${lib_vm_name} "-Wl,--export-dynamic")
    
    else()

        set(lib_vm_name "LibVM_${suffix}")
        add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}> )

        make_module(${lib_vm_name} USES ${vm_dependencies})
    
        set(exec_name "VM_${suffix}")
        add_executable(${exec_name} $<TARGET_OBJECTS:MainVM>)
        target_link_libraries(${exec_name} PRIVATE ${lib_vm_name})
    endif()

endfunction()

# Define suffixes and their respective compile definitions
add_vm_variant(tc "USE_TAIL_CALLS")
add_vm_variant(sc "USE_SWITCH_CASE")

# Default alias to "sc"

if (JIT_ENABLED)
    add_library(LibVM  $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:LibThreadVM_sc> $<TARGET_OBJECTS:JitDep>)
    target_link_libraries(LibVM ${llvm_libs} ${JitDep})
    make_module(LibVM USES llvm_helpers ${vm_dependencies})
    add_executable(VM $<TARGET_OBJECTS:MainVM> $<TARGET_OBJECTS:JitDep>)
    target_link_libraries(VM LibVM ${llvm_libs} ${JitDep})
else() # Same as for JIT, but without JITDep and llvm_libs
    add_library(LibVM  $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:LibThreadVM_sc>)
    make_module(LibVM USES ${vm_dependencies})
    add_executable(VM $<TARGET_OBJECTS:MainVM>)
    target_link_libraries(VM LibVM)
endif()

add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)