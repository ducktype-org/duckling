add_custom_test_pack(vm)

function(add_files)
	math(EXPR indices "${ARGC} - 1")

	foreach(index RANGE ${indices})
		set(tmp ${tmp} "${CMAKE_CURRENT_LIST_DIR}/${ARGV${index}}")
	endforeach()

	set(files ${files} ${tmp} PARENT_SCOPE)
endfunction()

# Add vm files
add_subdirectory(src/api)
add_subdirectory(src/events)
add_subdirectory(src/memory_data)
add_subdirectory(src/code_data)
add_subdirectory(src/services)
add_subdirectory(src/services_data)
add_subdirectory(src/supervisor)

function(add_vm suffix defines default)
  set(exec "Vm_${suffix}")

  set(lib "Lib${exec}")
  add_library(${lib} ${vm_sources})
  make_module(${lib} USES Base Listener Clap Crow::Crow Filesystem TPC JSON Result)
  target_compile_definitions(${lib} PRIVATE ${defines})

  set(sources src/main.cpp src/server.cpp src/cli.cpp)
  add_executable(${exec} ${sources})
  target_link_libraries(${exec} ${lib})

  if (default)
    add_executable(Vm ${sources})
    target_link_libraries(Vm ${lib})
    add_library(LibVm ALIAS ${lib})
  endif ()
endfunction()

add_vm("tc" "" ON)
add_vm("cg" "USE_COMPUTED_GOTO" OFF)
add_vm("sc" "USE_SWITCH_CASE" OFF)

add_subdirectory(playground)
add_subdirectory(tests)
