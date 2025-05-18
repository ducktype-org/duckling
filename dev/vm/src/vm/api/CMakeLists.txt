add_files(
		vm.cpp
		data/request.cpp
		data/api_error.cpp
)

set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
