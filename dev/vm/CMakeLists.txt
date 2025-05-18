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

add_library(LibCommonVM ${vm_common_sources})
make_module(LibCommonVM USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

add_library(LibThreadVM_TC ${vm_config_dependent_sources})
make_module(LibThreadVM_TC USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

add_library(LibThreadVM_SC ${vm_config_dependent_sources})
target_compile_definitions(LibThreadVM_SC PRIVATE "USE_SWITCH_CASE")
make_module(LibThreadVM_SC USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

add_library(LibThreadVM_CG ${vm_config_dependent_sources})
target_compile_definitions(LibThreadVM_CG PRIVATE "USE_COMPUTED_GOTO")
make_module(LibThreadVM_CG USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)

add_library(LibVM_tc INTERFACE)
target_link_libraries(LibVM_tc INTERFACE LibCommonVM LibThreadVM_TC)

add_library(LibVM_cg INTERFACE)
target_link_libraries(LibVM_cg INTERFACE LibCommonVM LibThreadVM_SC)

add_library(LibVM_sc INTERFACE)
target_link_libraries(LibVM_sc INTERFACE LibCommonVM LibThreadVM_CG)

add_library(LibVM ALIAS LibVM_tc)

set(sources src/vm/main.cpp src/vm/server.cpp src/vm/cli.cpp src/vm/vm_repl.cpp)

add_executable(VM_tc ${sources})
target_link_libraries(VM_tc LibVM_tc)

add_executable(VM_cg ${sources})
target_link_libraries(VM_cg LibVM_cg)

add_executable(VM_sc ${sources})
target_link_libraries(VM_sc LibVM_sc)

add_executable(VM ${sources})
target_link_libraries(VM LibVM)

add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)
