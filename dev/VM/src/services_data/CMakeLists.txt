add_files(
		type_metadata/type_metadata.cpp
		type_metadata/type.cpp
		memory/memory.cpp
)

set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
