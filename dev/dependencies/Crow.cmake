include(FetchContent)

set(CROW_COMMIT "f8c060c51feeca2c65828fb6f538603db4392d55")  # 1.3.2, released 2026-03-29

FetchContent_Declare(
	crow
	URL "https://github.com/CrowCpp/Crow/archive/${CROW_COMMIT}.zip"
	SYSTEM
)
FetchContent_MakeAvailable(crow)
