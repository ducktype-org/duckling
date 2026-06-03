#pragma once

#include "options.hpp"

#include <base/types/checked_okbad.hpp>

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
	 * @return Whether the initialization was successful or not.
	 * In case of failure, diagnostic messages will be reported in the global logger.
	 * Driver exit should still be called in the failure case.
	 */
	base::CheckedOkBad initializeTheCompiler(CompilerModeOfOperationAndOptions options);

	/**
	 * @brief initialize the global dia-int logger, used for reporting diagnostics during
	 * initialization phase and in other places outside of queries. This should be called at the
	 * very beginning of the initialization phase, before reporting any diagnostics
	 */
	void initializeGlobalLogger();
}
