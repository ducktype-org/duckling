include(FetchContent)

set(ASIO_COMMIT "89b0a4138a92883ae2514be68018a6c837a5b65f")

FetchContent_Declare(
	asio
	URL "https://github.com/chriskohlhoff/asio/archive/${ASIO_COMMIT}.zip"
	SYSTEM
)

FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")
