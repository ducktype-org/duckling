#pragma once

#include <typesystem/higher/symbol_type.hpp>

#include <variant>

namespace compiler::helios {
	// TODOP: What about the VmValue.
	using CompileTimeValue = std::variant<i64, bool, tsh::SymbolType<>>;
}
