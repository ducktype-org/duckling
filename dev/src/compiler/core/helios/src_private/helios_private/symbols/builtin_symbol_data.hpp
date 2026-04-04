#pragma once

#include <tsh/types.hpp>

namespace compiler::helios::builtin {
	/**
	 * Represents a built-in function, such as `builtin_output_i64` or `+` (integer addition).
	 */
	struct BuiltinFunctionData final {
		tsh::FunctionAbstractType type;

		explicit BuiltinFunctionData(const tsh::FunctionAbstractType type): type(type) {}
	};
}
