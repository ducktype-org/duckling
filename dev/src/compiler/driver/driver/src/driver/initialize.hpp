#pragma once

#include "options.hpp"

#include <base/types/ok_bad.hpp>

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
	 *
	 * @return Whether the initialization was successful or not. In case of failure, the error
	 * message will be printed to stderr.
	 */
	base::OkBad initializeTheCompiler(CompilerModeOfOperationAndOptions options);
}
