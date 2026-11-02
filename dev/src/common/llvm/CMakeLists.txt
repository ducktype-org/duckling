add_library(llvm_helpers INTERFACE)

target_include_directories(llvm_helpers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(llvm_helpers INTERFACE Base)
