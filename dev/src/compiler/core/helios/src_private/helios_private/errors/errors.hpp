#pragma once

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>

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
}
