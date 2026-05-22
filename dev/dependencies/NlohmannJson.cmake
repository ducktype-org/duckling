include(FetchContent)

set(NLOHMANN_JSON_TAG "v3.12.0")  # released 2025-04-11

FetchContent_Declare(
	nlohmann_json
	URL "https://github.com/nlohmann/json/releases/download/${NLOHMANN_JSON_TAG}/json.tar.xz"
	SYSTEM
)
FetchContent_MakeAvailable(nlohmann_json)
