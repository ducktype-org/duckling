include(FetchContent)

set(ASIO_COMMIT "89b0a4138a92883ae2514be68018a6c837a5b65f")
set(CROW_COMMIT "4f3f5deaaa01825c63c83431bfa96ccec195f741")

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
