include(FetchContent)

set(CROW_COMMIT "4f3f5deaaa01825c63c83431bfa96ccec195f741")

FetchContent_Declare(
	crow
	URL "https://github.com/CrowCpp/Crow/archive/${CROW_COMMIT}.zip"
	SYSTEM
)
FetchContent_MakeAvailable(crow)
