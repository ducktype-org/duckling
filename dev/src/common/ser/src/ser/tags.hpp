#pragma once

namespace ser {

	/**
	 * @brief The type an ADL serMake dispatches on: a function cannot be overloaded on its return
	 * type, so `serMake(ar)` alone could never say WHICH type to build. The tag also drags
	 * namespace ser into the associated set, so the barrier in internal/adl.hpp finds the hook.
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
