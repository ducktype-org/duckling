set(ALLOCATOR "DEFAULT" CACHE STRING "Specify the allocator type to use. Currently supports None (default) and mimalloc.")
set_property(CACHE ALLOCATOR PROPERTY STRINGS DEFAULT MIMALLOC)

# This is a proxy target for custom allocators, so we can link to it without
# worrying about the actual target name, if we want to switch allocators
# in the future.
# If needed, it can also be used to add some allocator specific source files or compile options in the future. 
add_library(allocator_proxy_library INTERFACE)

include(Mimalloc.cmake)
