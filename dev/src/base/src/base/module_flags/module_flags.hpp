/**
 * @brief Global flags of base module.
 * We want base to be stateless, and so
 * all flags used here are compile-time constants.
 * Modify this file to change the flags.
 */

#pragma once

namespace base {

	/**
	 * If set, base will output logs
	 * useful mostly in development and debugging.
	 */
	constexpr bool ENABLE_DEV_LOGS = false;
}
