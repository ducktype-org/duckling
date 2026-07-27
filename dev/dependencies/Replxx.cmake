include(FetchContent)
find_package(Git)

set(REPLXX_TAG "release-0.0.4")  # released 2021-10-21

FetchContent_Declare(
	replxx
	GIT_REPOSITORY https://github.com/AmokHuginnsson/replxx.git
	GIT_TAG        ${REPLXX_TAG}
	SYSTEM
    PATCH_COMMAND "${GIT_EXECUTABLE}" reset --hard HEAD
          COMMAND "${GIT_EXECUTABLE}" apply "${CMAKE_CURRENT_LIST_DIR}/patches/replxx-cmake-version.patch"
)

# Build replxx based on the selected mode
set(REPLXX_BUILD_MODE "STATIC" CACHE STRING "Build mode for replxx (STATIC, SHARED, DYNAMIC)")
set_property(CACHE REPLXX_BUILD_MODE PROPERTY STRINGS "STATIC" "SHARED" "DYNAMIC")

if(REPLXX_BUILD_MODE STREQUAL "STATIC")
	set(REPLXX_BUILD_STATIC ON CACHE BOOL "" FORCE)
	set(REPLXX_BUILD_SHARED OFF CACHE BOOL "" FORCE)
elseif(REPLXX_BUILD_MODE STREQUAL "SHARED" OR REPLXX_BUILD_MODE STREQUAL "DYNAMIC")
	set(REPLXX_BUILD_STATIC OFF CACHE BOOL "" FORCE)
	set(REPLXX_BUILD_SHARED ON CACHE BOOL "" FORCE)
else()
	message(FATAL_ERROR "Invalid REPLXX_BUILD_MODE: ${REPLXX_BUILD_MODE}")
endif()

set(REPLXX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(replxx)

# Suppress warnings in replxx sources that conflict with our strict -Werror flags.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(replxx PRIVATE
		-Wno-conversion -Wno-shadow -Wno-shadow=local
		-Wno-implicit-fallthrough
		-Wno-error=conversion -Wno-error=shadow=local -Wno-error=implicit-fallthrough
	)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	target_compile_options(replxx PRIVATE
		-Wno-conversion -Wno-shadow
		-Wno-implicit-fallthrough
		-Wno-error=conversion 
		-Wno-error=shadow
		-Wno-error=implicit-fallthrough
	)
else()
	message(FATAL_ERROR "Error: UNKNOWN COMPILER")
endif()
