namespace lexer::detail {
	class ICUDeinit final {
		~ICUDeinit();
	};

	/**
	 * @brief Clears ICU resources that are not cleared bynormal deinitialization.
	 */
	class ICUDeinitManager final {
		static ICUDeinit icu_deinit;
	};
}
