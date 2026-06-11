include(FetchContent)

set(PSAC_COMMIT "acec8165ee1a6046a10ff601f90b42e06613ea0b")

FetchContent_Declare(psac)
FetchContent_GetProperties(psac)
if(NOT psac_POPULATED)
	FetchContent_Populate(
		psac
		URL "https://github.com/poleszc/psac/archive/${PSAC_COMMIT}.zip"
		# Computation's in-class ctor/dtor instantiate unique_ptr<SPNode> before SPNode
		# is complete; GCC defers the instantiation, clang rejects it. Remove once the
		# fix lands upstream.
		# `|| true` keeps re-population idempotent (patch exits 1 when already applied).
		PATCH_COMMAND sh -c "patch -p1 --forward -i '${CMAKE_CURRENT_LIST_DIR}/psac_clang_computation_fix.patch' || true"
		SYSTEM
		QUIET
	)
endif()

add_library(psac INTERFACE)

target_include_directories(psac
	SYSTEM INTERFACE
	"${psac_SOURCE_DIR}/include"
)