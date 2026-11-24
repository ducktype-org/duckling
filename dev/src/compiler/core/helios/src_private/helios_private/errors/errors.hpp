// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios_private/errors/dia_interactive_elements.hpp>

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>

namespace compiler::helios {
	class SingleStmtFunctionMustBeExprError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "single_stmt_function_must_be_expr" };
		}

	public:
		SingleStmtFunctionMustBeExprError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ImmutableVariableNoInitError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "immutable_variable_no_init" };
		}

	public:
		ImmutableVariableNoInitError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	/** @brief A `const` declared without a value, e.g. `const x: i64;`. */
	class ConstWithoutValueError final: public dia::MessageWithCodeFragmentAndCause {
		[[nodiscard]] dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "const_without_value" };
		}

	public:
		ConstWithoutValueError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class DuplicateVariantAlternativeError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "duplicate_variant_alternative" };
		}

	public:
		DuplicateVariantAlternativeError(
			dia::StablePosition source_position, Box<InteractiveType> duplicated_type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::InteractiveArgument>("duplicated_type", std::move(duplicated_type));
		}
	};

	class CallNotCallableError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_not_callable" };
		}

	public:
		CallNotCallableError(dia::StablePosition source_position, Box<InteractiveType> callee_type):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::InteractiveArgument>("callee_type", std::move(callee_type));
		}
	};

	class InvalidMainReturnTypeError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "invalid_main_return_type" };
		}

	public:
		InvalidMainReturnTypeError(
			dia::StablePosition source_position, Box<InteractiveType> given_type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::InteractiveArgument>("given_type", std::move(given_type));
		}
	};

	class MissingUnaryOperatorFixityError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "missing_unary_operator_fixity" };
		}

	public:
		explicit MissingUnaryOperatorFixityError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	/**
	 * @brief A symbolic operator declared without a fixity takes no operands. Points at the
	 * parameter list.
	 */
	class OperatorWithoutOperandsError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "operator_without_operands" };
		}

	public:
		explicit OperatorWithoutOperandsError(dia::StablePosition params_position):
			  MessageWithCodeFragmentAndCause(params_position) {}
	};

	/**
	 * @brief Common base of the operator arity errors. Points at the parameter list and reports
	 * how many operands are declared.
	 */
	class OperatorArityError: public dia::MessageWithCodeFragmentAndCause {
	protected:
		/**
		 * @param declared_operands Operand count, including the implicit `self` of a non-static
		 * method.
		 * @param has_self Whether the operator is a non-static method, whose `self` is the first
		 * operand.
		 */
		OperatorArityError(dia::StablePosition params_position, u64 declared_operands, bool has_self):
			  MessageWithCodeFragmentAndCause(params_position) {
			addArgument<dia::TextArgument>("declared_operands", std::to_string(declared_operands));
			addArgument<dia::TextArgument>("has_self", has_self ? "true" : "false");
		}
	};

	/**
	 * @brief An operator marked `prefix` or `suffix` does not take exactly one operand.
	 */
	class InvalidUnaryOperatorArityError final: public OperatorArityError {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "invalid_unary_operator_arity" };
		}

	public:
		InvalidUnaryOperatorArityError(
			dia::StablePosition params_position, u64 declared_operands, bool has_self
		):
			  OperatorArityError(params_position, declared_operands, has_self) {}
	};

	/**
	 * @brief An operator marked `infix` does not take exactly two operands, or a symbolic operator
	 * declared without a fixity takes more than two.
	 */
	class InvalidBinaryOperatorArityError final: public OperatorArityError {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "invalid_binary_operator_arity" };
		}

	public:
		InvalidBinaryOperatorArityError(
			dia::StablePosition params_position, u64 declared_operands, bool has_self
		):
			  OperatorArityError(params_position, declared_operands, has_self) {}
	};

	class ReturnWithoutValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "return_without_value" };
		}

	public:
		ReturnWithoutValueError(
			dia::StablePosition source_position, Box<InteractiveType> expected_type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::InteractiveArgument>("expected_type", std::move(expected_type));
		}
	};

	/**
	 * @brief Error for a statement that only makes sense inside a function body (an expression,
	 * `return`, `if`, ...) but appears at module level.
	 */
	class StatementOutsideFunctionError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "statement_outside_function" };
		}

	public:
		explicit StatementOutsideFunctionError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
