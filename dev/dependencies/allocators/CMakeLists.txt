option(ALLOCATOR "Specify the allocator type to use. Currently supports None (default) and mimalloc." "DEFAULT")

# This is a proxy target for custom allocators, so we can link to it without
# worrying about the actual target name, if we want to switch allocators
# in the future. 
add_library(allocator_library INTERFACE)

include(Mimalloc.cmake)
