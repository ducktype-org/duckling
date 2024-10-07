include(FetchContent)
include(FindOrBuildICU.cmake)
include(BuildTreeSitter.cmake)

# It's nice to get stripped-down release zips instead of cloning
# a repo and checking out a tag
set(NLOHMANN_JSON_TAG "v3.11.3")
set(RESULT_COMMIT "a97e66bc66f140132619fd5f5cdffc30127f4220")
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
)

FetchContent_Declare(
	result
	URL "https://github.com/bitwizeshift/result/archive/${RESULT_COMMIT}.zip"
)

FetchContent_Declare(
	asio
	URL "https://github.com/chriskohlhoff/asio/archive/${ASIO_COMMIT}.zip"
)

FetchContent_Declare(
	crow
	URL "https://github.com/CrowCpp/Crow/archive/${CROW_COMMIT}.zip"
)

FetchContent_Declare(
	base64
	URL "https://github.com/tobiaslocker/base64/archive/${BASE64_COMMIT}.zip"
)

# @CMAKE 25: change to usage of FetchContent_Declare SYSTEM parameter
function(setAsSystemLibrary libname)
	get_target_property(TEMP ${libname} INTERFACE_INCLUDE_DIRECTORIES)
	set_target_properties(${libname} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${TEMP}")
endfunction()

FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")

FetchContent_MakeAvailable(crow)
setAsSystemLibrary(Crow)

FetchContent_MakeAvailable(nlohmann_json)
setAsSystemLibrary(nlohmann_json)

FetchContent_MakeAvailable(result)
setAsSystemLibrary(Result)

find_library(gvc gvc)

FindOrBuildICU(VERSION 74.1 URL "${ICU_RELEASE}" URL_HASH "${ICU_CONTROL}")

# Do not call MakeAvailable, because
# We want to avoid using their CMakeLists.txt
FetchContent_GetProperties(base64)
if(NOT base64_POPULATED)
	FetchContent_Populate(base64)
endif()

add_library(base64 INTERFACE)
target_include_directories(base64 INTERFACE "${base64_SOURCE_DIR}/include")
setAsSystemLibrary(base64)

BuildTreeSitter(
	TS_URL "https://github.com/tree-sitter/tree-sitter/archive/${TREE_SITTER_COMMIT}.zip"
	TS_CPP_URL "https://github.com/tree-sitter/tree-sitter-cpp/archive/${TREE_SITTER_CPP_COMMIT}.zip"
)

# This macro inserts LLVM as a library to the project. It should be used in the top-level
# CMakeList.txt file.
# First it tries to find a downloaded version of LLVM located somewhere in `scripts/downloads`,
# and upon failing tries to use a system version of LLVM which is at least 18.1.x.
macro(insertLLVM)
	find_package(LLVM 18.1.0 CONFIG PATHS scripts/downloads NO_DEFAULT_PATH)
	if (NOT ${LLVM_FOUND} EQUAL 1)
		message(STATUS "Custom LLVM not found, looking for local")
		find_package(LLVM 18.1.0 REQUIRED CONFIG)
	else()
		message(STATUS "Using custom LLVM")
	endif ()
	message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
	message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
	# @TODO: Mark all LLVM components as system with setAsSystemLibraries
endmacro()
