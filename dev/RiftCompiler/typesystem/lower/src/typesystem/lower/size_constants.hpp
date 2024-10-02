#pragma once

#include <base/ints.hpp>

namespace tsl {
	// This may become const instead of constexpr because it might be defined during runtime.
	constexpr usize META_SIZE    = 64;
	constexpr usize POINTER_SIZE = 64;
	constexpr usize BYTE_SIZE    = 8;
	constexpr usize BOOL_SIZE    = BYTE_SIZE;
	constexpr usize CHAR_SIZE    = BYTE_SIZE;
}
