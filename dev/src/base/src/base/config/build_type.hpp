#pragma once

namespace base {
#if defined(BUILD_TYPE_DEV)
	constexpr bool IS_BUILD_TYPE_DEV     = true;
	constexpr bool IS_BUILD_TYPE_RELEASE = false;
#elif defined(BUILD_TYPE_RELEASE)
	constexpr bool IS_BUILD_TYPE_DEV     = false;
	constexpr bool IS_BUILD_TYPE_RELEASE = true;
#else
	#error "Unknown BUILD_TYPE"
#endif
}
