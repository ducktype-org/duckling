#pragma once

#include <string>
#include <base/define_helper.hpp>
#include <variant>
#include "helios_result.hpp"

namespace compiler::helios::errors {
	struct SymbolNotFoundError {};

	struct AmbiguityError {};

	struct ExpressionParsingError {};
}
