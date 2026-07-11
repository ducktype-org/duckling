add_files(
	vc.cpp
	epoch.cpp
	shadow_memory.cpp
	fast_track_globals.cpp
)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)
