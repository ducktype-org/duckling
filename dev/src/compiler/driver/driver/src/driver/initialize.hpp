#pragma once

#include "options.hpp"

#include <base/types/ok_bad.hpp>

namespace compiler::driver {

	/**
	 * Helper struct used to wrap the result of the initialization,
	 * in a way that forces the caller to check it (to avoid silent failures).
	 *
	 * If status method is never called, the destructor will panic.
	 */
	struct InitializationResult final {
	private:
		base::OkBad result;
		bool        checked = false;

	public:
		InitializationResult(base::OkBad result);
		InitializationResult(const InitializationResult&) = delete;
		InitializationResult(InitializationResult&&)      = delete;

		~InitializationResult();

		[[nodiscard]]
		base::OkBad status();
	};

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
	InitializationResult initializeTheCompiler(CompilerModeOfOperationAndOptions options);
}
