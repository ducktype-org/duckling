add_subdirectory(copy-and-patch)

set(vm_common_sources ${vm_common_sources} PARENT_SCOPE)
set(vm_config_dependent_sources ${vm_config_dependent_sources} PARENT_SCOPE)
set(jit_dependency ${jit_dependency} PARENT_SCOPE)
