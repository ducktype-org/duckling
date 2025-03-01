/**
 * @file init_guard.hpp
 * @brief Intention of this library is to provide automatic way of
 * detecting if initialization function caused cycle
 * and to detect if initialization function was already called before.
 */
#pragma once

#include "exceptions.hpp"

namespace base::detail {
	enum class InitState : unsigned char {
		NotInitialized,
		Initializing,
		Initialized,
	};

	void logInitFunction(const char* function_name);
}

/**
 * @brief Use at the beginning of an init as a guard.
 * Panics if function is called, while its other "instance" is still running.
 * Stops the function if it was already called before.
 * @important This macro should always be used at the beginning of the function,
 * and SIMPLE_INIT_GUARD_END should be used at the end.
 */
#define SIMPLE_INIT_GUARD_BEGIN                                                 \
	::base::detail::logInitFunction(__PRETTY_FUNCTION__);                       \
	static ::base::detail::InitState _detail_init_state                         \
		= ::base::detail::InitState::NotInitialized;                            \
                                                                                \
	if (_detail_init_state == ::base::detail::InitState::NotInitialized) {      \
		_detail_init_state = ::base::detail::InitState::Initializing;           \
	} else if (_detail_init_state == ::base::detail::InitState::Initializing) { \
		CORE_PANIC("Cycle detected in initialization function!");               \
	} else {                                                                    \
		return;                                                                 \
	}


#define SIMPLE_INIT_GUARD_END _detail_init_state = ::base::detail::InitState::Initialized;
