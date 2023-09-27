include(FetchContent)

# It's nice to get stripped-down release zips instead of cloning
# a repo and checking out a tag
set(JSON_STRUCT_COMMIT "e3045af90ff331c97cc8f71374f558ae13409d9f")
set(RESULT_COMMIT "a97e66bc66f140132619fd5f5cdffc30127f4220")

FetchContent_Declare(
		json_struct
		URL "https://github.com/jorgen/json_struct/archive/${JSON_STRUCT_COMMIT}.zip"
)

FetchContent_Declare(
		result
		URL "https://github.com/bitwizeshift/result/archive/${RESULT_COMMIT}.zip"
)

FetchContent_MakeAvailable(json_struct)
FetchContent_MakeAvailable(result)
