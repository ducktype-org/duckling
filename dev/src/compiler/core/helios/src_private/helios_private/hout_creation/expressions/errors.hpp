// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file errors.hpp
 * @brief Errors and error messages related to helios expression processing.
 */
#pragma once

#include <helios_private/errors/dia_interactive_elements.hpp>

#include <diagnostic/message.hpp>

#include <string>

namespace compiler::helios::code {

	class UndefinedBinaryOperatorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "undefined_binary_operator" };
		}

	public:
		UndefinedBinaryOperatorError(
			dia::StablePosition  source_position,
			std::string          op,
			Box<InteractiveType> lhs_type,
			Box<InteractiveType> rhs_type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::TextArgument>("operator", std::move(op));
			addArgument<dia::InteractiveArgument>("lhs_type", std::move(lhs_type));
			addArgument<dia::InteractiveArgument>("rhs_type", std::move(rhs_type));
		}
	};

	/**
	 * @brief An identifier was used as an infix operator (`a foo b`), but none of the functions it
	 * names is declared `infix`.
	 */
	class OperatorNotDeclaredInfixError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "operator_not_declared_infix" };
		}

	public:
		OperatorNotDeclaredInfixError(dia::StablePosition source_position, std::string op):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::TextArgument>("operator", std::move(op));
		}
	};

	/**
	 * @brief Attached to `OperatorNotDeclaredInfixError`: points at the name of a function that
	 * would match, if it were declared `infix`.
	 */
	class OperatorDeclaredWithoutInfixNote final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "operator_declared_without_infix" };
		}

	public:
		OperatorDeclaredWithoutInfixNote(dia::StablePosition name_position, std::string op):
			  MessageWithCodeFragmentAndCause(name_position) {
			addArgument<dia::TextArgument>("operator", std::move(op));
		}
	};

	class UndefinedUnaryOperatorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "undefined_unary_operator" };
		}

	public:
		UndefinedUnaryOperatorError(
			dia::StablePosition source_position, std::string op, Box<InteractiveType> type
		):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::TextArgument>("operator", std::move(op));
			addArgument<dia::InteractiveArgument>("type", std::move(type));
		}
	};

	class InvalidNumericLiteralError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "invalid_numeric_literal" };
		}

	public:
		InvalidNumericLiteralError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class NumericLiteralTooLargeError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "numeric_literal_too_large" };
		}

	public:
		NumericLiteralTooLargeError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class LiteralDoesNotFitError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "literal_does_not_fit" };
		}

	public:
		LiteralDoesNotFitError(dia::StablePosition source_position, std::string type_desc):
			  MessageWithCodeFragmentAndCause(source_position) {
			addArgument<dia::TextArgument>("type_desc", std::move(type_desc));
		}
	};

}
