include(FetchContent)

set(rang_TAG "1756145b0244bbdef3fd88ba921fed6258065b28")

FetchContent_Declare(
  rang
  GIT_REPOSITORY https://github.com/wojtek-rz/rang.git
  GIT_TAG       ${rang_TAG}
)

FetchContent_MakeAvailable(rang)
