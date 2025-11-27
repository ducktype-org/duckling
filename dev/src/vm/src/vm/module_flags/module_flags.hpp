#pragma once

namespace vm {
#ifdef BUILD_TYPE_DEV_DEBUG
	/**
	 * Whether detailed VM logging is enabled.
	 * This is a compile-time constant for performance reasons.
	 */
	constexpr bool ENABLE_VM_DETAIL_LOGGING = true;
#else
	constexpr bool ENABLE_VM_DETAIL_LOGGING = false;
#endif
}
