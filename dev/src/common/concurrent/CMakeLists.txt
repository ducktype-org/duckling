add_library(Concurrent
	src/concurrent/concurrent_allocator.hpp
)

make_module(Concurrent USES Base)

# duck_add_test(common concurrent tests/concurrent_test.cpp USES Concurrent
# 	INCLUDE "${CMAKE_CURRENT_LIST_DIR}/src_private")

add_example(concurrent_example examples/concurrent_example.cpp
	USES Concurrent Base)

# add target sources:
target_sources(Concurrent PRIVATE
	examples/hide.cpp)