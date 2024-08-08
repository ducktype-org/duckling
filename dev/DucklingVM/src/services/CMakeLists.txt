add_files(profiler/profiler.cpp
		allocator/allocator.cpp
		allocator/stack_allocator.cpp
		executor_f8/executor.cpp
		preprocessor_f8/parser/parser.cpp
		preprocessor_f8/preprocessor.cpp)

set(ducklingvm_sources ${ducklingvm_sources} ${files} PARENT_SCOPE)
