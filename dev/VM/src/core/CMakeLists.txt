add_files(
		supervisor/supervisor.cpp
		process/process.cpp
		thread/thread.cpp
)

set(vm_sources ${vm_sources} ${files} PARENT_SCOPE)
