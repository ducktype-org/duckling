add_files(
		supervisor/supervisor.cpp
		thread/vmthread.cpp
		process/vmprocess.cpp
		process/proc_io.cpp
		process/type_metadata/type_metadata.cpp
		process/type_metadata/type.cpp
		process/memory/memory.cpp
		process/memory/memory_data/block.cpp
		process/memory/allocator/allocator.cpp
		process/memory/allocator/stack_allocator.cpp
)

set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
