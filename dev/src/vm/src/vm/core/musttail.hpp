#pragma once

#include <base/config/target_info.hpp>

#if defined(__has_cpp_attribute) && __has_cpp_attribute(clang::musttail)
	#define MUST_TAIL [[clang::musttail]]
// GCC exposes no capability probe for gnu::musttail, so keep the version gate.
#elif BASE_TARGET_COMPILER_GCC && __GNUG__ >= 15
	#define MUST_TAIL [[gnu::musttail]]
#else
	#define MUST_TAIL

	#ifdef USE_TAIL_CALLS
		#warning \
			"USE_TAIL_CALLS without support from compiler. This can potentially cause stack-overflow."
	#endif
#endif
