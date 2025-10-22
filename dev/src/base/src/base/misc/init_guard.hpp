/**
 * @file init_guard.hpp
 * @brief Intention of this library is to provide automatic way of
 * detecting if initialization function caused cycle
 * and to detect if initialization function was already called before.
 */
#pragma once

#include <base/except/exceptions.hpp>

namespace base::internal {
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
#define SIMPLE_INIT_GUARD_BEGIN                                                   \
	static ::base::internal::InitState _detail_init_state                         \
		= ::base::internal::InitState::NotInitialized;                            \
                                                                                  \
	if (_detail_init_state == ::base::internal::InitState::NotInitialized) {      \
		_detail_init_state = ::base::internal::InitState::Initializing;           \
	} else if (_detail_init_state == ::base::internal::InitState::Initializing) { \
		CORE_PANIC("Cycle detected in initialization function!");                 \
	} else {                                                                      \
		return;                                                                   \
	}                                                                             \
	::base::internal::logInitFunction(__PRETTY_FUNCTION__);


#define SIMPLE_INIT_GUARD_END _detail_init_state = ::base::internal::InitState::Initialized;
