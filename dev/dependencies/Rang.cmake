include(FetchContent)

set(rang_TAG "v3.2")

FetchContent_Declare(
  rang
  GIT_REPOSITORY https://github.com/agauniyal/rang.git
  GIT_TAG 	     ${rang_TAG}
)
FetchContent_MakeAvailable(rang)