
namespace base::detail {
	struct ICUDeinit final {
		~ICUDeinit();
	};

	/**
	 * @brief Clears resources that are not cleared bynormal deinitialization like specific library
	 * resources (ICU)
	 */
	struct GlobalDeinit final {
		static ICUDeinit icu_deinit;
	};
}
