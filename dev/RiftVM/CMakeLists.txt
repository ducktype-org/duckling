add_custom_test_pack(vm)

function(add_files)
    math(EXPR indices "${ARGC} - 1")
    foreach (index RANGE ${indices})
        set(tmp ${tmp} "${CMAKE_CURRENT_LIST_DIR}/${ARGV${index}}")
    endforeach ()
    set(files ${files} ${tmp} PARENT_SCOPE)
endfunction()

# Dependencies
add_subdirectory(dependencies)

# Add vm files
add_subdirectory(src/api)
add_subdirectory(src/events)
add_subdirectory(src/memory_data)
add_subdirectory(src/code_data)
add_subdirectory(src/services)
add_subdirectory(src/services_data)
add_subdirectory(src/supervisor)

add_library(LibRiftVm ${riftvm_sources})
target_include_directories(LibRiftVm PUBLIC "${CMAKE_CURRENT_LIST_DIR}/libs/asio/asio/include")

make_module(LibRiftVm USES Base Listener Clap Crow::Crow Filesystem TPC JSON)

add_executable(RiftVm src/main.cpp src/server.cpp src/cli.cpp)
target_link_libraries(RiftVm LibRiftVm)

add_subdirectory(playground)
add_subdirectory(tests)
