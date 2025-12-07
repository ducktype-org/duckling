include(FetchContent)

set(rang_TAG "f55210ce27c0fa87601526102904c2702256a8a5")

FetchContent_Declare(
  rang
  GIT_REPOSITORY https://github.com/AleBiCi/rang.git
  GIT_TAG       ${rang_TAG}
)

FetchContent_MakeAvailable(rang)
