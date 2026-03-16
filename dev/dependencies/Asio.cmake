include(FetchContent)

set(ASIO_COMMIT "03cf5f86a780dd102f1cdd3a59d1244d12143e46")  # 1.38.0, released 2025-10-30

FetchContent_Declare(
	asio
	URL "https://github.com/chriskohlhoff/asio/archive/${ASIO_COMMIT}.zip"
	SYSTEM
)

FetchContent_MakeAvailable(asio)
set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include")
