#pragma once

#include <functional>
#include <base/define_helper.hpp>

namespace init {
	/**
	 * @brief Registers a function to be called during initialization.
	 */
	void registerForInit(std::function<void()>);

	/**
	 * @brief Registers a function to be called during deinitialization.
	 */
	void registerForDeinit(std::function<void()>);

	/**
	 * Construction of this object causes all functions
	 * passed to registerForInit to be called.
	 * Destruction of this object causes all functions
	 * passed to registerForDeinit to be called.
	 *
	 * It is intended to be used as a
	 * local variable in a main function or other
	 * function effectively acting as one.
	 *
	 * @note It should not be used as a global object
	 * as it could lead to static destruction order fiasco (yes you read that right).
	 * The module can in principle be modified to handle this case,
	 * but it is not currently implemented, as we see no good reason for it.
	 */
	struct InitObject final {
		InitObject();
		~InitObject();
	};
}

/**
 * Runs peace of code during static initialization phase.
 * Note that it is completely independent from InitObject.
 * @note This macro should only be used in global/namespace scope.
 * @note This macro should only be used in cpp files, to avoid duplication.
 * @note For technical reasons the macro should be used at most once per line.
 * Unfortunately C++26 `_` identifier don't work here, as it is a global variable.
 */
#define RUN_BEFORE_MAIN(code) \
namespace {                                                                \
	int CONCAT_2(run_before_main_helper_JG8MG9_, __LINE__) = []() noexcept -> int { \
		code; \
		return 0;                                                          \
	}();                                                                   \
}


