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

set(vm_internal_deps
    Base Clah Events Filesystem TPC Init Logger Diagnostic
)

set(vm_external_deps
    Crow::Crow JSON
)

set(vm_dependencies
    ${vm_internal_deps}
    ${vm_external_deps}
)

add_library(LibCommonVM OBJECT ${vm_common_sources})
make_module(LibCommonVM USES ${vm_dependencies})

add_library(MainVM OBJECT ${exec_sources})
make_module(MainVM USES ${vm_dependencies})

if (JIT_ENABLED) # Setup JitDep - opcode definitions and all of its dependencies to allow full LTO later.

    # Setup list of all bitcode dependencies (from outside of VM).
    set(vm_dep_bc_targets "") # Targets for dependency.
    set(vm_dep_objects_bc "") # Actuall list.

    # Dummy source so header-only libraries (Events) do not fail
    set(DUMMY_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/dummy.c")
    if(NOT EXISTS "${DUMMY_SOURCE}")
        file(WRITE "${DUMMY_SOURCE}" "")
    endif()

    foreach(dep ${vm_internal_deps})
        if(TARGET ${dep})
            add_library(${dep}_bc OBJECT "${DUMMY_SOURCE}" $<TARGET_OBJECTS:${dep}>)
            target_compile_options(${dep}_bc PRIVATE -emit-llvm)

            list(APPEND vm_dep_bc_targets ${dep}_bc)
            list(APPEND vm_dep_objects_bc $<TARGET_OBJECTS:${dep}_bc>)
        endif()
    endforeach()

    # Link llvm it will be ONLY used by JitDep.
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

    add_library(JitDep OBJECT
        ${jit_dependency}
    )
    make_module(JitDep USES llvm_helpers Base Diagnostic)
    target_compile_options(JitDep PRIVATE
        --embed-dir=${CMAKE_BINARY_DIR}
    )
    target_include_directories(JitDep PRIVATE ${LLVM_INCLUDE_DIRS})

    target_include_directories(MainVM PRIVATE $<TARGET_PROPERTY:JitDep,INTERFACE_INCLUDE_DIRECTORIES>)

    # Setup list of all bitcode dependencies (from inside of VM - libcommon).
    add_library(LibCommonVM_bc OBJECT ${vm_common_sources})
    make_module(LibCommonVM_bc USES ${vm_dependencies})
    target_compile_options(LibCommonVM_bc PRIVATE -emit-llvm)
endif()

# Create the LibVM_{sc/tc} static libraries and the VM_{sc/tc} executables.
# In the middle it creates the LibThreadVM_{sc/tc} object libraries.
# The configuration depends on the suffix and the compile_def.
function(add_vm_variant suffix compile_def)
    set(lib_thread_vm_name "LibThreadVM_${suffix}")
    set(lib_vm_name "LibVM_${suffix}")
    set(exec_name "VM_${suffix}")

    add_library(${lib_thread_vm_name} OBJECT ${vm_config_dependent_sources})
    target_compile_definitions(${lib_thread_vm_name} PRIVATE ${compile_def})

    if (JIT_ENABLED)
        # Additionall llvm dependecies.
        make_module(${lib_thread_vm_name} USES llvm_helpers ${vm_dependencies})
        target_include_directories(${lib_thread_vm_name} PRIVATE ${LLVM_INCLUDE_DIRS})
        target_link_libraries(${lib_thread_vm_name} ${llvm_libs})

        # Setup list of all bitcode dependencies (from inside of VM - dependant to VM variant).
        add_library(${lib_thread_vm_name}_bc OBJECT ${vm_config_dependent_sources})
        make_module(${lib_thread_vm_name}_bc USES llvm_helpers ${vm_dependencies})
        target_include_directories(${lib_thread_vm_name}_bc PRIVATE ${LLVM_INCLUDE_DIRS})
        target_link_libraries(${lib_thread_vm_name}_bc ${llvm_libs})
        target_compile_options(${lib_thread_vm_name}_bc PRIVATE -emit-llvm)
        target_compile_definitions(${lib_thread_vm_name}_bc PRIVATE ${compile_def})

        set(common_bc ${CMAKE_CURRENT_BINARY_DIR}/common_${suffix}.bc)
        set(extracted_func_file ${CMAKE_CURRENT_BINARY_DIR}/extracted_function_${suffix}.txt)

        # Pipeline preparing opcodes definitions for JIT.
        add_custom_command(
            OUTPUT ${common_bc} ${extracted_func_file}
            # Links all opcodes together, without optimizations.
            COMMAND ${LLVM_LINK}
                $<TARGET_OBJECTS:LibCommonVM_bc>
                $<TARGET_OBJECTS:${lib_thread_vm_name}_bc>
                $<TARGET_OBJECTS:cp-stencils_bc>
                ${vm_dep_objects_bc}
                -o ${common_bc}
            COMMAND_EXPAND_LISTS

            # Get all microopcodes names to add them to api later. Heuristic used: contains "OpFuns", "op_" and not "debug"
            COMMAND /usr/bin/env python3
                ${CMAKE_SOURCE_DIR}/scripts/py/jit/jitable_interface.py
                ${LLVM_TOOLS_ARGS}
                -i ${common_bc}
                -o ${extracted_func_file}

            # Strip global constructors, they are not necessery for jit.
            COMMAND ${LLVM_EXTRACT} --delete --glob=llvm.global_ctors  ${common_bc} -o ${common_bc}

            # Passess used to shorten bytecode and reduce JIT startup. Public api list, represents all jitable opcodes.
            # globalopt removes unused global variables. It is crucial to do it first to clean up after global constcors
            # got stripped in the previous step.
            # Internalize marks all function not in api list as internal, so they possibly can be removed in dead code
            # elimination (part of O3)
            # Other passess can improve bitcode (they are not part of O3).
            # O3 is duplicated, because it can possibly improve code after strip-dead-prototypes. It is important to generate
            # as short bitcode as possible, even at the cost of build time.
            COMMAND ${LLVM_OPT}
                -passes=globalopt,internalize,attributor,simplifycfg,default<O3>,strip-dead-prototypes,default<O3>
                --internalize-public-api-file=${extracted_func_file}
                ${common_bc}
                -o ${common_bc}

            VERBATIM
            DEPENDS $<TARGET_OBJECTS:LibCommonVM_bc>
                    cp-stencils_bc
                    ${CMAKE_SOURCE_DIR}/scripts/py/jit/jitable_interface.py
                    $<TARGET_OBJECTS:${lib_thread_vm_name}_bc>
                    # Here dependency to both bc files and targets is necessery.
                    ${vm_dep_objects_bc} # File dependency ensures each file will tracked and program will be rebuild if it changes.
                    ${vm_dep_bc_targets} # Target dependency says what should be build first.
        )
        # JitDep depends to both bc files (it can embed both of them), but is NOT config dependent.
        add_custom_target(CommonVMBitcode_${suffix} DEPENDS ${common_bc} ${extracted_func_file})
        add_dependencies(JitDep CommonVMBitcode_${suffix})
        set_target_properties(CommonVMBitcode_${suffix}
            PROPERTIES
                VM_COMMON_BC ${common_bc}
        )

        add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}> $<TARGET_OBJECTS:JitDep>)

        target_link_libraries(${lib_vm_name} ${llvm_libs})

        make_module(${lib_vm_name} USES llvm_helpers ${vm_dependencies})

        target_compile_options(JitDep PRIVATE
            "--embed-dir=$<TARGET_PROPERTY:JitDep,BINARY_DIR>"
        )

        add_executable(${exec_name} $<TARGET_OBJECTS:MainVM> $<TARGET_OBJECTS:JitDep>)
        target_link_libraries(${exec_name} PRIVATE ${lib_vm_name})
        target_link_options(${exec_name} PRIVATE -Wl,--export-dynamic)

    else()
        make_module(${lib_thread_vm_name} USES ${vm_dependencies})

        add_library(${lib_vm_name} $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:${lib_thread_vm_name}> )

        make_module(${lib_vm_name} USES ${vm_dependencies})
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
    make_module(LibVM USES llvm_helpers ${vm_dependencies})
    target_link_libraries(LibVM ${llvm_libs})
    add_executable(VM $<TARGET_OBJECTS:MainVM> $<TARGET_OBJECTS:JitDep>)

    target_link_libraries(VM LibVM)
    target_link_options(VM PRIVATE -Wl,--export-dynamic)
else() # Same as for JIT, but without JITDep and llvm_libs
    add_library(LibVM  $<TARGET_OBJECTS:LibCommonVM> $<TARGET_OBJECTS:LibThreadVM_sc>)
    make_module(LibVM USES ${vm_dependencies})
    add_executable(VM $<TARGET_OBJECTS:MainVM>)

    target_link_libraries(VM LibVM)
endif()

add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)
