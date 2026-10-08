#pragma once

namespace ser {

	/**
	 * @brief Tells an ADL `serMake` which type to build: `serMake(ar, ser::Tag<T>{})`. A
	 * function cannot be overloaded on its return type alone, so the type has to be an
	 * argument.
	 */
	template<class T>
	struct Tag final {
		using type = T;
	};

	struct RawInitTag final {
		explicit RawInitTag() = default;
	};

	inline constexpr RawInitTag RAW_INIT{};

	struct InPlaceTag final {
		explicit InPlaceTag() = default;
	};

	inline constexpr InPlaceTag IN_PLACE{};

	struct ViewTag final {
		explicit ViewTag() = default;
	};

	struct SizedTag final {
		explicit SizedTag() = default;
	};

	struct UnsizedTag final {
		explicit UnsizedTag() = default;
	};

	inline constexpr UnsizedTag UNSIZED_TAG{};

	struct BytesTag final {
		explicit BytesTag() = default;
	};

	inline constexpr BytesTag BYTES_TAG{};

}  // namespace ser
