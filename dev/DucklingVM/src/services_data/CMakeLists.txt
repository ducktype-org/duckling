add_files(
		type_metadata/type_metadata.cpp
		type_metadata/type.cpp
		memory/memory.cpp
)

set(ducklingvm_sources ${ducklingvm_sources} ${files} PARENT_SCOPE)
