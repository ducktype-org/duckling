// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/preproc/cat.hpp>

#include <functional>

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
	 * as it could lead to static destruction order fiasco (yes you read that right)
	 * as well as to calling registerForInit after init was already done.
	 * The module can in principle be modified to handle this case,
	 * but it is not currently implemented, as we see no good reason for it.
	 */
	struct InitObject final {
		InitObject();
		~InitObject();
	};

	/**
	 * Returns true if InitObject was constructed.
	 * Should be used for sanity-checks only.
	 */
	bool wasInitObject();
}

/**
 * Runs piece of code during static initialization phase.
 *
 * It is usually used with `registerForInit` function, like this:
 * `RUN_BEFORE_MAIN(init::registerForInit(some_init_func));`
 *
 * If you place this macro in CPP file it will run:
 * * once if given cpp files is used,
 * * zero times if cpp file is never used (this is due to how linker behaves).
 *
 * If you place this macro in header file it will run:
 * * once per each (used) translation unit that includes the given header.
 *
 * When used for inits it is generally safer to put it in header files (e.g. some_module/init.hpp),
 * and include this header file in every cpp/hpp file that requires init to be done.
 *
 * Putting it in cpp file is will work most of the time, but you need to be aware of the fact that
 * it will not work if the cpp file is not used. It is usually best to schedule inits this way only
 * when we are dealing with single-file (hpp+cpp) module
 *
 * @note This is completely independent from InitObject.
 * @note This macro should only be used in global/namespace scope.
 * @note For technical reasons the macro should be used at most once per line.
 * Unfortunately C++26 `_` identifier don't work here, as it is a global variable.
 */
#define RUN_BEFORE_MAIN(code)                                                      \
	namespace {                                                                    \
		int CAT(run_before_main_helper_JG8MG9_, __LINE__) = []() noexcept -> int { \
			code;                                                                  \
			return 0;                                                              \
		}();                                                                       \
	}
