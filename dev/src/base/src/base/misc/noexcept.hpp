#pragma once

#include <base/config/build_type.hpp>

/**
 * @brief Function will not throw in Release build
 */
#define NOEXCEPT noexcept(::base::BUILD_TYPE_RELEASE)
