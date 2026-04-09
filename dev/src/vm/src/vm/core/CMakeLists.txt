if(JIT_ENABLED)
	add_subdirectory(jit)
endif()

add_files(
	supervisor/supervisor.cpp
	process/safe_vmprocess.cpp
	process/vmprocess.cpp
	process/proc_io.cpp
	process/builtin_functions.cpp
	process/type_metadata/type_metadata.cpp
	process/type_metadata/type.cpp
	process/memory/memory.cpp
	process/concurrency/gil.cpp
	process/concurrency/condition_variable.cpp
	process/concurrency/synchronization_primitives.cpp
	thread/ivmthread.cpp
)

set(vm_common_sources ${vm_common_sources} ${files} PARENT_SCOPE)

add_files(
	thread/opcode_functions/opcodes_functions_impl_exec.cpp
	thread/opcode_functions/opcodes_functions_impl_debug.cpp
	thread/low_program/instruction.cpp
	thread/safe_vmthread.cpp
	thread/vmvalue.cpp
	thread/vmvalueref.cpp
	jit/jit_compiler.cpp
)

set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)

add_files(
	jit/opcodes_bitcode_source.cpp
)

set(jit_dependency ${jit_dependency} ${files} PARENT_SCOPE)
