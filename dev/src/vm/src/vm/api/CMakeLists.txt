add_files(
		vm.cpp
		data/api_error.cpp
		data/response.cpp
)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
