include(FetchContent)

set(CROW_COMMIT "4b21a399e0b5e7666cc0b2d25202845200e5e923")  # 1.3.1, released 2026-02-12

FetchContent_Declare(
	crow
	URL "https://github.com/CrowCpp/Crow/archive/${CROW_COMMIT}.zip"
	SYSTEM
)
FetchContent_MakeAvailable(crow)
