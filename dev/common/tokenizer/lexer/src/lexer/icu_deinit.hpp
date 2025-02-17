#pragma once

namespace lexer::detail {
	class ICUDeinit final {
	public:
		~ICUDeinit();
	};

	/**
	 * @brief Clears ICU resources that are not cleared bynormal deinitialization.
	 */
	class ICUDeinitManager final {
		static ICUDeinit icu_deinit;
	};
}
