add_subdirectory(llvm/)
add_subdirectory(copy-and-patch/)

add_files(
    cf_analysis.cpp
    cf_graph.cpp
    jit_helper.cpp
)
set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)
set(vm_common_sources ${vm_common_sources} PARENT_SCOPE)
set(jit_dependency ${jit_dependency} PARENT_SCOPE)
