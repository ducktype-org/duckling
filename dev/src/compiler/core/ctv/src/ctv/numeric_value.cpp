#include "ctv/numeric_value.hpp"

#include "base/str/str_utils.hpp"
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string>
#include <type_traits>

namespace compiler::numeric_value {
	NumericValue::NumericValue() = default;

	const NumericValue::Storage& NumericValue::getStorage() const { return value; }

	[[nodiscard]] std::string NumericValue::toString() const {
		return std::visit(
			[&](auto&& value) {
				using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, f128>)
					// No overload of std::to_string exists for __Float128, thus we cast it.
					return base::toString(static_cast<f64>(value));
				else
					return base::toString(value);
			},
			value
		);
	}
}
