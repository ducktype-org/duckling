set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w") # TODO: This should be more isolated
add_subdirectory(clients)
add_subdirectory(common)
add_subdirectory(view_manager)