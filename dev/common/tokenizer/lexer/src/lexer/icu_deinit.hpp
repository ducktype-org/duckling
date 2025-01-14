namespace lexer::detail {
	struct ICUDeinit final {
		~ICUDeinit();
	};

	/**
	 * @brief Clears ICU resources that are not cleared bynormal deinitialization.
	 */
	struct ICUDeinitManager final {
		static ICUDeinit icu_deinit;
	};
}
