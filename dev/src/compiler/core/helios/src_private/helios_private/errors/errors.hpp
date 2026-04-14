#pragma once

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/stable_position.hpp>

namespace compiler::helios {
	class SingleStmtFunctionMustBeExprError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "single_stmt_function_must_be_expr" };
		}

	public:
		SingleStmtFunctionMustBeExprError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class ImmutableVariableNoInitError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "immutable_variable_no_init" };
		}

	public:
		ImmutableVariableNoInitError(dia_int::StablePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
