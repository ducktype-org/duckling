include(FetchContent)

set(PSAC_COMMIT "acec8165ee1a6046a10ff601f90b42e06613ea0b")

FetchContent_Declare(psac)
FetchContent_GetProperties(psac)
if(NOT psac_POPULATED)
	FetchContent_Populate(
		psac
		URL "https://github.com/poleszc/psac/archive/${PSAC_COMMIT}.zip"
		SYSTEM
		QUIET
	)
endif()

add_library(psac INTERFACE)

target_include_directories(psac
	SYSTEM INTERFACE
	"${psac_SOURCE_DIR}/include"
)