#pragma once

#include <diagnostic/message.hpp>

namespace pst {
	class InvalidControlFlowTargetError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "invalid_control_flow_target" };
		}

	public:
		InvalidControlFlowTargetError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class ControlFlowTargetArgumentsError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "control_flow_target_arguments" };
		}

	public:
		ControlFlowTargetArgumentsError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnexpectedControlFlowTargetTokensError final:
		  public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "unexpected_control_flow_target_tokens" };
		}

	public:
		UnexpectedControlFlowTargetTokensError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MissingControlFlowTargetSemicolonError final:
		  public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "missing_control_flow_target_semicolon" };
		}

	public:
		MissingControlFlowTargetSemicolonError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadSpecifierCallError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_specifier_call" };
		}

	public:
		BadSpecifierCallError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class InvalidExternContentWarning final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "warning",
				     .family        = "parser",
				     .name          = "invalid_extern_content" };
		}

	public:
		InvalidExternContentWarning(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};
}
