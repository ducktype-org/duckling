#pragma once

#include <base/config/target_info.hpp>

/**
 * @brief `[[no_unique_address]]` that also takes effect on MSVC and clang-cl.
 * @note Both follow the MSVC ABI, which ignores the standard spelling, so only
 * `[[msvc::no_unique_address]]` lets an empty member take no space there.
 */
#if BASE_TARGET_COMPILER_MSVC || BASE_TARGET_COMPILER_CLANG_CL
	#define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
	#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
