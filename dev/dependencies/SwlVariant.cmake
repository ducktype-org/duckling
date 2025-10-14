include(FetchContent)

set(SWL_VARIANT_COMMIT "23f9929e7781be3885a62b59045e967eb410c45d")

FetchContent_Declare(swlvariant)
FetchContent_GetProperties(swlvariant)

if(NOT swlvariant_POPULATED)
  FetchContent_Populate(
    swlvariant
    URL "https://github.com/groundswellaudio/swl-variant/archive/${SWL_VARIANT_COMMIT}.zip"
    SYSTEM
    QUIET
  )
endif()

add_library(swlvariant INTERFACE)

target_include_directories(swlvariant SYSTEM INTERFACE
  ${swlvariant_SOURCE_DIR}/include/
)
