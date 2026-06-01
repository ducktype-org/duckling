#pragma once

namespace compiler::repl {
#ifdef USE_REPLXX
	/**
	 * @brief Default completion toggle for the REPL frontend.
	 */
	constexpr bool FRONTEND_DEFAULT_COMPLETIONS_ENABLED = true;

	/**
	 * @brief Default bracketed paste toggle for the REPL frontend.
	 * https://en.wikipedia.org/wiki/Bracketed-paste
	 */
	constexpr bool FRONTEND_DEFAULT_BRACKETED_PASTE_ENABLED = true;
#else
	/**
	 * @brief Default completion toggle for the REPL frontend.
	 */
	constexpr bool FRONTEND_DEFAULT_COMPLETIONS_ENABLED = false;

	/**
	 * @brief Default bracketed paste toggle for the REPL frontend.
	 * https://en.wikipedia.org/wiki/Bracketed-paste
	 */
	constexpr bool FRONTEND_DEFAULT_BRACKETED_PASTE_ENABLED = true;
#endif
}
