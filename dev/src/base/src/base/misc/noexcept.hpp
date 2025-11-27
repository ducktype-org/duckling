#pragma once

#include <base/config/build_type.hpp>

/**
 * @brief Function will not throw in Release build
 */
#define RELEASE_NOEXCEPT noexcept(::base::IS_BUILD_TYPE_RELEASE)
