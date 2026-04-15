add_library(Artifacts src_private/artifacts/artifacts.cpp)

set(ARTIFACTS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen)

# Regenerate build_id.hpp on every build. configure_file (run inside the script)
# only rewrites the header when the content changes, so untouched sources do not
# cause a rebuild cascade.
add_custom_target(artifacts_build_id ALL
	COMMAND ${CMAKE_COMMAND}
		-DSRC_ROOT=${SRC_PATH}
		-DGEN_DIR=${ARTIFACTS_GEN_DIR}
		-DTEMPLATE=${CMAKE_CURRENT_SOURCE_DIR}/src/artifacts/build_id.hpp.in
		-P ${CMAKE_CURRENT_SOURCE_DIR}/compute_build_id.cmake
	BYPRODUCTS ${ARTIFACTS_GEN_DIR}/artifacts/build_id.hpp
	COMMENT "Computing artifacts build id"
	VERBATIM
)
add_dependencies(Artifacts artifacts_build_id)

target_include_directories(Artifacts PUBLIC ${ARTIFACTS_GEN_DIR})

make_module(Artifacts USES Base Filesystem Hashing Logger Concurrent)

add_example(artifacts_example examples/artifacts_example.cpp
	USES Artifacts)

duck_add_test(common artifacts tests/artifacts_test.cpp USES Artifacts)
