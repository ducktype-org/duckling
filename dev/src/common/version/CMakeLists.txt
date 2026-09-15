add_library(Version
	src/version/version.cpp
)

make_module(Version USES Base)

# The commit date has to describe the commit the binary is actually built from, so it is collected
# on every build instead of once at configure time (a configured-in date goes stale the moment you
# commit again). The generator writes the header through `copy_if_different`, so this only ever
# recompiles the single translation unit that includes it, and only when the date really changed.
set(version_generated_dir ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(version_commit_info_header ${version_generated_dir}/version/commit_info.hpp)

add_custom_target(VersionCommitInfo
	BYPRODUCTS ${version_commit_info_header}
	COMMAND ${CMAKE_COMMAND}
		-D OUTPUT_FILE=${version_commit_info_header}
		-D GIT_WORK_DIR=${CMAKE_CURRENT_LIST_DIR}
		-P ${CMAKE_CURRENT_LIST_DIR}/GenerateCommitInfo.cmake
	COMMENT "Collecting Duckling commit information"
	VERBATIM
)

add_dependencies(Version VersionCommitInfo)
target_include_directories(Version PRIVATE ${version_generated_dir})

# Everything that is fixed for a given configuration is baked in here, so that only the commit date
# needs a generated header.
target_compile_definitions(Version PRIVATE
	DUCKLING_SEMVER="${DUCKLING_VERSION_STRING}"
	DUCKLING_SEMVER_MAJOR="${Duckling_VERSION_MAJOR}"
	DUCKLING_SEMVER_MINOR="${Duckling_VERSION_MINOR}"
	DUCKLING_SEMVER_PATCH="${Duckling_VERSION_PATCH}"
	DUCKLING_PRERELEASE="${DUCKLING_VERSION_PRERELEASE}"
	DUCKLING_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
	DUCKLING_HOST_SYSTEM="${CMAKE_HOST_SYSTEM_NAME}"
	DUCKLING_HOST_PROCESSOR="${CMAKE_HOST_SYSTEM_PROCESSOR}"
	DUCKLING_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
	DUCKLING_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
	# No licence has been chosen for Duckling yet. Both stay empty until one is, and the renderers
	# skip empty values, so nothing has to change here besides the two strings.
	DUCKLING_LICENSE_NAME=""
	DUCKLING_COPYRIGHT_LINE=""
)
