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
				     .family        = "type_check",
				     .name          = "missing_unary_operator_fixity" };
		}

	public:
		explicit MissingUnaryOperatorFixityError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class InvalidOperatorFixityError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "invalid_operator_fixity" };
		}

	public:
		explicit InvalidOperatorFixityError(dia::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
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
}
