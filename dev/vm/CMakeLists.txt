add_custom_test_pack(vm)

function(add_files)
  math(EXPR indices "${ARGC} - 1")

  foreach(index RANGE ${indices})
    set(tmp ${tmp} "${CMAKE_CURRENT_LIST_DIR}/${ARGV${index}}")
  endforeach()

  set(files ${files} ${tmp} PARENT_SCOPE)
endfunction()

# Add vm files
add_subdirectory(src/vm)

function(duck_add_vm suffix defines default)
  set(exec "VM_${suffix}")

  set(lib "Lib${exec}")
  add_library(${lib} ${vm_sources})
  make_module(${lib} USES Base Listener Clap Crow::Crow Filesystem TPC JSON Init)
  target_compile_definitions(${lib} PRIVATE ${defines})

  set(sources src/vm/main.cpp src/vm/server.cpp src/vm/cli.cpp src/vm/vm_repl.cpp)
  add_executable(${exec} ${sources})
  target_link_libraries(${exec} ${lib})

  if(default)
    add_executable(VM ${sources})
    target_link_libraries(VM ${lib})
    add_library(LibVM ALIAS ${lib})
  endif()
endfunction()

duck_add_vm("tc" "" ON)
duck_add_vm("cg" "USE_COMPUTED_GOTO" OFF)
duck_add_vm("sc" "USE_SWITCH_CASE" OFF)

add_subdirectory(playground)
add_subdirectory(tests)

duck_add_test(vm program_builders tests/program_builders_test.cpp USES LibVM)
