include(FetchContent)
include(FindOrBuildICU.cmake)

# It's nice to get stripped-down release zips instead of cloning
# a repo and checking out a tag
set(JSON_STRUCT_COMMIT "e3045af90ff331c97cc8f71374f558ae13409d9f")
set(RESULT_COMMIT "a97e66bc66f140132619fd5f5cdffc30127f4220")
set(ASIO_COMMIT "89b0a4138a92883ae2514be68018a6c837a5b65f")
set(CROW_COMMIT "4f3f5deaaa01825c63c83431bfa96ccec195f741")

# ICU supporting Unicode 15.1
set(ICU_RELEASE "https://github.com/unicode-org/icu/releases/download/release-74-1/icu4c-74_1-src.tgz")
set(ICU_CONTROL "SHA512=32c28270aa5d94c58d2b1ef46d4ab73149b5eaa2e0621d4a4c11597b71d146812f5e66db95f044e8aaa11b94e99edd4a48ab1aa8efbe3d72a73870cd56b564c2")

FetchContent_Declare(
		json_struct
		URL "https://github.com/jorgen/json_struct/archive/${JSON_STRUCT_COMMIT}.zip"
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


FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" PARENT_SCOPE)

FetchContent_MakeAvailable(crow)

FetchContent_MakeAvailable(json_struct)

FetchContent_MakeAvailable(result)

FindOrBuildICU(VERSION 74.1 URL "${ICU_RELEASE}" URL_HASH "${ICU_CONTROL}")
