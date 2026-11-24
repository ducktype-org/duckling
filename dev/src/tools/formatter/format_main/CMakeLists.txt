add_executable(duckfmt src/main.cpp)
target_link_libraries(duckfmt Printer Formatter TokenSource Filesystem Clah Base Init nlohmann_json::nlohmann_json)

# We want duckfmt to use custom allocator if specified.
target_link_libraries(duckfmt allocator_proxy_library)
