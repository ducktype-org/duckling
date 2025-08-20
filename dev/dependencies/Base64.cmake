include(FetchContent)

set(BASE64_COMMIT "387b32f337b83d358ac1ffe574e596ba99c41d31")

FetchContent_Declare(base64)
FetchContent_GetProperties(base64)
if(NOT base64_POPULATED)
	FetchContent_Populate(
		base64
		URL "https://github.com/tobiaslocker/base64/archive/${BASE64_COMMIT}.zip"
		SYSTEM
		QUIET
	)
endif()

add_library(base64 INTERFACE)

target_include_directories(base64
	SYSTEM INTERFACE
	"${base64_SOURCE_DIR}/include"
)
