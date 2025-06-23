include(FetchContent)
include(FindOrBuildICU.cmake)
include(BuildTreeSitter.cmake)

# It's nice to get stripped-down release zips instead of cloning
# a repo and checking out a tag
set(NLOHMANN_JSON_TAG "v3.11.3")
set(ASIO_COMMIT "89b0a4138a92883ae2514be68018a6c837a5b65f")
set(CROW_COMMIT "4f3f5deaaa01825c63c83431bfa96ccec195f741")
set(BASE64_COMMIT "387b32f337b83d358ac1ffe574e596ba99c41d31")

# Tree sitter for parsing cpp
set(TREE_SITTER_COMMIT "12fb31826b8469cc7b9788e72bceee5af1cf0977")
set(TREE_SITTER_CPP_COMMIT "30f973c2244f0bff444186185f475c3bd76bc3a5")

# ICU supporting Unicode 15.1
set(ICU_RELEASE "https://github.com/unicode-org/icu/releases/download/release-74-1/icu4c-74_1-src.tgz")
set(ICU_CONTROL "SHA512=32c28270aa5d94c58d2b1ef46d4ab73149b5eaa2e0621d4a4c11597b71d146812f5e66db95f044e8aaa11b94e99edd4a48ab1aa8efbe3d72a73870cd56b564c2")

FetchContent_Declare(
	nlohmann_json
	URL "https://github.com/nlohmann/json/releases/download/${NLOHMANN_JSON_TAG}/json.tar.xz"
	SYSTEM
)

FetchContent_Declare(
	asio
	URL "https://github.com/chriskohlhoff/asio/archive/${ASIO_COMMIT}.zip"
	SYSTEM
)

FetchContent_Declare(
	crow
	URL "https://github.com/CrowCpp/Crow/archive/${CROW_COMMIT}.zip"
	SYSTEM
)

FetchContent_Declare(
	base64
)

FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")

FetchContent_MakeAvailable(crow)

FetchContent_MakeAvailable(nlohmann_json)

find_library(GVC_LIBRARY gvc)
find_library(CGRAPH_LIBRARY cgraph)
find_library(CDT_LIBRARY cdt)

set(GRAPHVIZ_LIBRARIES_FOUND FALSE)

if(GVC_LIBRARY AND CGRAPH_LIBRARY AND CDT_LIBRARY)
    set(GRAPHVIZ_LIBRARIES_FOUND TRUE)

    add_library(System::gvc UNKNOWN IMPORTED GLOBAL)
    set_target_properties(System::gvc PROPERTIES
        IMPORTED_LOCATION "${GVC_LIBRARY}"
    )

    add_library(System::cgraph UNKNOWN IMPORTED GLOBAL)
    set_target_properties(System::cgraph PROPERTIES
        IMPORTED_LOCATION "${CGRAPH_LIBRARY}"
    )

    add_library(System::cdt UNKNOWN IMPORTED GLOBAL)
    set_target_properties(System::cdt PROPERTIES
        IMPORTED_LOCATION "${CDT_LIBRARY}"
    )
endif()

set(GRAPHVIZ_LIBRARIES_FOUND ${GRAPHVIZ_LIBRARIES_FOUND} PARENT_SCOPE)

# stdc++exp library (experimental C++ library)
add_library(system_stdcxxexp INTERFACE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(system_stdcxxexp INTERFACE "-lstdc++exp")
endif()

FindOrBuildICU(VERSION 74.1 URL "${ICU_RELEASE}" URL_HASH "${ICU_CONTROL}")

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

BuildTreeSitter(
	TS_URL "https://github.com/tree-sitter/tree-sitter/archive/${TREE_SITTER_COMMIT}.zip"
	TS_CPP_URL "https://github.com/tree-sitter/tree-sitter-cpp/archive/${TREE_SITTER_CPP_COMMIT}.zip"
)

# This macro inserts LLVM as a library to the project. It should be used in the top-level
# CMakeList.txt file.
# First it tries to find a downloaded version of LLVM located somewhere in `scripts/downloads`,
# and upon failing tries to use a system version of LLVM which is at least 19.1.x.
macro(insertLLVM)
	find_package(LLVM 19.1.0 CONFIG PATHS scripts/downloads NO_DEFAULT_PATH)
	if (NOT ${LLVM_FOUND} EQUAL 1)
		message(STATUS "Custom LLVM not found, looking for local")
		find_package(LLVM 19.1.0 REQUIRED CONFIG)
	else()
		message(STATUS "Using custom LLVM")
	endif ()
	message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
	message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
	# @TODO: Mark all LLVM components as system with setAsSystemLibraries
endmacro()
