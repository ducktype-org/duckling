/**
 * @file init_guard.hpp
 * @brief Intention of this library is to provide automatic way of
 * detecting if initialization function caused cycle
 * (either Panic should be thrown or reinit should be prevented),
 * and to detect if initialization function was already called before.
 *
 * @attention Current implementation just ignores secondary
 * inits after the first finished.
 */
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
