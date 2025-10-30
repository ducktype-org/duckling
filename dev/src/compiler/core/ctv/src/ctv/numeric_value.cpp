#pragma once

#include "ctv/numeric_value.hpp"
#include "base/str/str_utils.hpp"
#include <base/comptime/type_traits.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <string>

namespace compiler::numeric_value {
	NumericValue::NumericValue() = default;

	const NumericValue::Storage& NumericValue::getStorage() const { return value; }

	[[nodiscard]] std::string NumericValue::toString() const { return base::toString(value); }
}
