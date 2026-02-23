include(FetchContent)

set(NLOHMANN_JSON_TAG "v3.12.0")

FetchContent_Declare(
    nlohmann_json
    URL "https://github.com/nlohmann/json/archive/refs/tags/${NLOHMANN_JSON_TAG}.zip"
)

FetchContent_MakeAvailable(nlohmann_json)

# Sposób użycia w projekcie:
# target_link_libraries(twoja_target_nazwa PRIVATE nlohmann_json::nlohmann_json)