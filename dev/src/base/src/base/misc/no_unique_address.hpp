#pragma once

/**
 * @brief `[[no_unique_address]]` that also takes effect on MSVC.
 * @note MSVC accepts the standard spelling but ignores it to keep its ABI, only
 * `[[msvc::no_unique_address]]` lets an empty member take no space there.
 */
#if defined(_MSC_VER)
	#define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
	#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif
