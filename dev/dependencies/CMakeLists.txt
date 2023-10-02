include(FetchContent)

# It's nice to get stripped-down release zips instead of cloning
# a repo and checking out a tag
set(JSON_STRUCT_COMMIT "e3045af90ff331c97cc8f71374f558ae13409d9f")
set(RESULT_COMMIT "a97e66bc66f140132619fd5f5cdffc30127f4220")
set(ASIO_COMMIT "89b0a4138a92883ae2514be68018a6c837a5b65f")
set(CROW_COMMIT "4f3f5deaaa01825c63c83431bfa96ccec195f741")

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

FetchContent_MakeAvailable(crow)

FetchContent_MakeAvailable(json_struct)

FetchContent_MakeAvailable(result)
