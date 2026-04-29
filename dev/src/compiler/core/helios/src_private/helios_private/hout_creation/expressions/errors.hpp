/**
 * @file errors.hpp
 * @brief Errors and error messages related to helios expression processing.
 */
#pragma once

#include <diagnostic_interactive/message.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

#include <string>

namespace compiler::helios::code {

	class UndefinedBinaryOperatorError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "undefined_binary_operator" };
		}

	public:
		UndefinedBinaryOperatorError(
			dia_int::StablePosition source_position,
			std::string             op,
			Box<InteractiveType>    lhs_type,
			Box<InteractiveType>    rhs_type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia_int::TextArgument>("operator", std::move(op));
			addArgument<dia_int::InteractiveArgument>("lhs_type", std::move(lhs_type));
			addArgument<dia_int::InteractiveArgument>("rhs_type", std::move(rhs_type));
		}
	};

	class UndefinedUnaryOperatorError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "undefined_unary_operator" };
		}

	public:
		UndefinedUnaryOperatorError(
			dia_int::StablePosition source_position, std::string op, Box<InteractiveType> type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia_int::TextArgument>("operator", std::move(op));
			addArgument<dia_int::InteractiveArgument>("type", std::move(type));
		}
	};

	class InvalidNumericLiteralError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "invalid_numeric_literal" };
		}

	public:
		InvalidNumericLiteralError(dia_int::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class NumericLiteralTooLargeError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "numeric_literal_too_large" };
		}

	public:
		NumericLiteralTooLargeError(dia_int::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class LiteralDoesNotFitError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "literal_does_not_fit" };
		}

	public:
		LiteralDoesNotFitError(dia_int::StablePosition source_position, std::string type_desc):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia_int::TextArgument>("type_desc", std::move(type_desc));
		}
	};

}
