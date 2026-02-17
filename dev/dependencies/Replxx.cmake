include(FetchContent)

set(REPLXX_TAG "release-0.0.4")

FetchContent_Declare(
	replxx
	GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
	GIT_TAG        ${REPLXX_TAG}
	SYSTEM
)

# Build replxx as a static library
set(REPLXX_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(REPLXX_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(replxx)

# Suppress warnings in replxx sources that conflict with our strict -Werror flags.
target_compile_options(replxx PRIVATE
	-Wno-conversion -Wno-shadow -Wno-shadow=local
	-Wno-implicit-fallthrough
	-Wno-error=conversion -Wno-error=shadow=local -Wno-error=implicit-fallthrough
)
