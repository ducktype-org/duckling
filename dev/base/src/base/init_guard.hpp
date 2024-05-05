#pragma once

/**
 * @brief Use at the beginning of an init as a guard.
 */
#define RIFT_SIMPLE_INIT_GUARD_BEGIN      \
	static bool _detail_was_init = false; \
	if (_detail_was_init) { return; }

/**
 * @brief Use at the end of an init as a guard.
 */
#define RIFT_SIMPLE_INIT_GUARD_END _detail_was_init = true;
