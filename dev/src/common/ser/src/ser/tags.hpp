#pragma once

namespace ser {

	/**
	 * @brief Tells an ADL `serMake` which type to build: `serMake(ar, ser::tag<T>{})`. A
	 * function cannot be overloaded on its return type alone, so the type has to be an
	 * argument.
	 */
	template<class T>
	struct tag final {
		using type = T;
	};

	struct raw_init_t final {
		explicit raw_init_t() = default;
	};

	inline constexpr raw_init_t RAW_INIT{};

	struct in_place_t final {
		explicit in_place_t() = default;
	};

	inline constexpr in_place_t IN_PLACE{};

	struct view_t final {
		explicit view_t() = default;
	};

	struct sized_t final {
		explicit sized_t() = default;
	};

	struct unsized_t final {
		explicit unsized_t() = default;
	};

	inline constexpr unsized_t UNSIZED_TAG{};

	struct bytes_t final {
		explicit bytes_t() = default;
	};

	inline constexpr bytes_t BYTES_TAG{};

} /* namespace ser */
