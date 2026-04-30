add_files(
	llvm_compiler.cpp
	jit_utils.cpp
	absolute_symbols.cpp
	jit_data.cpp
)
set(vm_config_dependent_sources ${vm_config_dependent_sources} ${files} PARENT_SCOPE)

add_files(
	opcodes_bitcode_source.cpp
)
set(jit_dependency ${jit_dependency} ${files} PARENT_SCOPE)
