/**
 * @file builtinoperators.hpp
 * @brief query builtin operators on int (in the future not only int)
 */

#pragma once

#include <exec/ctv.hpp>
#include <operations/operation.hpp>

#include <vector>

namespace exec {

	// @TODO: unify with lang_def(key_spec_op.hpp)
	enum class Operator {
		Plus,
		Minus,
		Asterisk,
		Slash,
		ExclamationMark,
	};

	struct BuiltInOp {
		Operator                    op;
		std::vector<ts::TypeDesc<>> parameters;

		auto operator<=>(const BuiltInOp& other) const = default;
	};

	using BuiltInOpMap = base::Map<BuiltInOp, operation::OperationId>;

	BuiltInOpMap& getBuiltInOps();

	void initBuiltInOps();
}
