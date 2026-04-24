# Define custom build types
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

set(VALID_BUILD_TYPES Dev DevDebug DevOpt Release ReleaseOpt Debug Perf) 

if(NOT "${CMAKE_BUILD_TYPE}" IN_LIST VALID_BUILD_TYPES)
  message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
endif()

if (CMAKE_BUILD_TYPE MATCHES ".*Opt.*|.*Perf.*")
  set(BUILD_TYPE_IS_OPTIMISED ON)
else()
  set(BUILD_TYPE_IS_OPTIMISED OFF)
endif()
