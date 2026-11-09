add_files(
		vm.cpp
		data/api_error.cpp
		data/function_source_info.cpp
		data/response.cpp
		data/stack_trace.cpp
)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
