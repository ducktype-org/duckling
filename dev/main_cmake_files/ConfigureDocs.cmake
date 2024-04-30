# We still want to compile project,
# if docs dependencies are not present (e.g. in GitHub workflows).
# Add `-D BUILD_DOCS=OFF` option to CMake to ignore docs
option(BUILD_DOCS "Build test programs" ON)

if(BUILD_DOCS)
	add_subdirectory(docs)

	# add_custom_target(docs-clean make -s -f CmakeFiles/Makefile2 docs/clean)
	# add_custom_target(docs-clean ninja -t clean docs)
endif(BUILD_DOCS)
