include(FetchContent)

set(rang_TAG "v3.2")

FetchContent_Declare(
  rang
  URL https://github.com/agauniyal/rang/archive/${rang_TAG}.tar.gz
  SYSTEM
)

FetchContent_MakeAvailable(rang)
