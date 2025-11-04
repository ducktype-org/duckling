#pragma once

namespace base {
	/**
	 * Type to be used instead of bool, when
	 * ok/bad makes more sense then true/false.
	 * It's a struct instead of plain enum to allow methods.
	 */
	struct OkBad final {
		enum class OkBadEnum : bool { Ok, Bad };
		OkBadEnum value;

		[[nodiscard]]
		constexpr bool isOk() const {
			return value == OkBadEnum::Ok;
		}

		[[nodiscard]]
		constexpr bool isBad() const {
			return not isOk();
		}
	};

	constexpr OkBad OK  = OkBad{ OkBad::OkBadEnum::Ok };
	constexpr OkBad BAD = OkBad{ OkBad::OkBadEnum::Bad };
}
