add_subdirectory(api)
add_subdirectory(bytecode)
add_subdirectory(core)
add_subdirectory(debugger)
add_subdirectory(loader)

set(vm_common_sources ${vm_common_sources} PARENT_SCOPE)
set(vm_config_dependent_sources ${vm_config_dependent_sources} PARENT_SCOPE)
set(jit_dependency ${jit_dependency} PARENT_SCOPE)
