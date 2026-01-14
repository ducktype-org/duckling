add_files(
		supervisor/supervisor.cpp
		process/vmprocess.cpp
		process/proc_io.cpp
		process/builtin_functions.cpp
		process/type_metadata/type_metadata.cpp
		process/type_metadata/type.cpp
		process/memory/memory.cpp
)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)

add_files(
	thread/opcode_functions/opcodes_functions_impl_exec.cpp
	thread/opcode_functions/opcodes_functions_impl_debug.cpp
	thread/low_program/instruction.cpp
	thread/vmthread.cpp
    thread/vmvalue.cpp
)

<<<<<<< HEAD
set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE) 

add_files(
	jit/opcode_definitions.cpp
)

set(jit_dependency ${jit_dependency} ${files} PARENT_SCOPE) 
=======
set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)
>>>>>>> c33eee918 ([DVM] Fix, Remove: Remove mutex from memory module, simplify mov_* operations, fix bugs (#1808))
