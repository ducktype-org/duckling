include(FetchContent)
find_package(Git)

set(rang_TAG "22345aa4c468db3bd4a0e64a47722aad3518cc81")

FetchContent_Declare(
  rang
  GIT_REPOSITORY https://github.com/agauniyal/rang.git
  GIT_TAG       ${rang_TAG}
  PATCH_COMMAND "${GIT_EXECUTABLE}" reset --hard HEAD
        COMMAND "${GIT_EXECUTABLE}" apply "${CMAKE_CURRENT_LIST_DIR}/patches/rang-cmake-version.patch"
)

FetchContent_MakeAvailable(rang)
