add_files(
		vm.cpp
		data/request.cpp
		data/api_error.cpp
)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
