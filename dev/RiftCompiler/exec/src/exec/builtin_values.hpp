/**
 * @file builtin_values.hpp
 * @brief not implemented
 */

#pragma once

#include "ctv.hpp"

namespace exec {
	// value of void:
	CTV noneValue();

	// value of null-pointer (null == none in Rift itself, but they have to be separate here since
	// they have different size and types)
	CTV nullValue();
}
