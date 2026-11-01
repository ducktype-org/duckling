#pragma once

#include <helios_private/errors/dia_interactive_elements.hpp>

#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>

#include <string>
#include <string_view>

namespace compiler::helios {
	class NoEnclosingLoopError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_enclosing_loop" };
		}

	public:
		NoEnclosingLoopError(dia::StablePosition position, std::string_view action):
			  MessageWithCodeFragmentAndCause(position) {
			addArgument<dia::TextArgument>("action", std::string(action));
		}
	};

	class NoNamedControlFlowTargetError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_named_control_flow_target" };
		}

	public:
		NoNamedControlFlowTargetError(
			dia::StablePosition position, std::string_view action, std::string_view name
		):
			  MessageWithCodeFragmentAndCause(position) {
			addArgument<dia::TextArgument>("action", std::string(action));
			addArgument<dia::TextArgument>("name", std::string(name));
		}
	};

	class NoControlFlowKindTargetError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_control_flow_kind_target" };
		}

	public:
		NoControlFlowKindTargetError(
			dia::StablePosition position, std::string_view action, std::string_view kind
		):
			  MessageWithCodeFragmentAndCause(position) {
			addArgument<dia::TextArgument>("action", std::string(action));
			addArgument<dia::TextArgument>("kind", std::string(kind));
		}
	};

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
