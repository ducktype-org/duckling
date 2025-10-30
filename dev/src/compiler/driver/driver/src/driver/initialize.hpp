#pragma once

#include "options.hpp"

namespace compiler::driver {

	/**
	 * @brief Initializes the compiler with the given options.
	 * This function initializes the query state, query input,
	 * frontend, and some other global states.
	 * It also handles incremental compilation "boilerplate".
	 * It is generally required to call this function to use the compiler,
	 * in standard usage scenarios.
	 *
	 * @note In the future this function may return some kind of handle
	 * that will be used to interact with top-level driver operations
	 * such as handling change in the source code input.
	 */
	void initializeTheCompiler(CompilerModeOfOperationAndOptions options);

	/**
	 * @brief Test helper: resets the initialization state of the compiler
	 * so that tests can call `initializeTheCompiler` again in the same process.
	 */
	void resetInitializationForTests();
}
