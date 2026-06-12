if (JIT_ENABLED)
	add_subdirectory(jit/)
	set(jit_dependency ${jit_dependency} PARENT_SCOPE)
endif ()

add_files(
	supervisor/supervisor.cpp
	safe/safe_vmprocess.cpp
	process/vmprocess.cpp
	process/lifecycle.cpp
	process/proc_io.cpp
	builtin_functions.cpp
	safe/type_metadata/type_metadata.cpp
	safe/type_metadata/type.cpp
	safe/memory/memory.cpp
	safe/concurrency/deadlock_detection.cpp
	safe/concurrency/gil.cpp
	safe/concurrency/condition_variable.cpp
	safe/concurrency/synchronization_primitives.cpp
	thread/ivmthread.cpp
)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)

add_files(
	safe/opcode_functions/opcodes_functions_impl_exec.cpp
	safe/opcode_functions/opcodes_functions_impl_debug.cpp
	safe/low_program/instruction.cpp
	safe/low_program/cfg/cf_graph.cpp
	safe/low_program/cfg/cf_analysis.cpp
	safe/safe_vmthread.cpp
	vmvalue/vmvalue.cpp
	vmvalue/vmvalueref.cpp
)

set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)
set(jit_dependency ${jit_dependency} PARENT_SCOPE)
