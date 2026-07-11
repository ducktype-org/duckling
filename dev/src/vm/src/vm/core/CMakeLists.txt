if (JIT_ENABLED)
	add_subdirectory(jit/)
	set(jit_dependency ${jit_dependency} PARENT_SCOPE)
endif ()

add_files(
	supervisor/supervisor.cpp
	safe/safe_vmprocess.cpp
	process/ivmprocess.cpp
	process/proc_io.cpp
	thread/ivmthread.cpp
	builtin_functions.cpp
	native/dynamic_library.cpp
	safe/type_metadata/type_metadata.cpp
	safe/type_metadata/type.cpp
	safe/memory/initialization_from_const.cpp
	safe/memory/memory.cpp
	safe/concurrency/deadlock_detection.cpp
	safe/concurrency/gil.cpp
	safe/concurrency/condition_variable.cpp
	safe/concurrency/synchronization_primitives.cpp

	fast/fast_vmprocess.cpp
	fast/program/type.cpp
)

add_subdirectory(process/concurrency/fast_track)

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
	fast/eval/instruction_functions.cpp
	fast/program/relocator.cpp
	fast/fast_vmthread.cpp
	fast/program/instructions/executable.cpp
)

set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)
set(jit_dependency ${jit_dependency} PARENT_SCOPE)
