#pragma once

#include <diagnostic_interactive/message.hpp>

namespace pst {
	class UnrecognizedPatternInCaseError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "unrecognized_pattern_in_case_error" };
		}

	public:
		UnrecognizedPatternInCaseError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class RoundExprStartError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "round_expr_start_error" };
		}

	public:
		RoundExprStartError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MatchCaseWithNoBodyError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "match_case_no_body" };
		}

	public:
		MatchCaseWithNoBodyError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class DoubleDefaultBranchError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "double_default_branch" };
		}

	public:
		DoubleDefaultBranchError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnconditionedBranchAfterConditionedError final:
		  public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "unconditioned_branch_after_conditioned" };
		}

	public:
		UnconditionedBranchAfterConditionedError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class AttrStarError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "attr_star_error" };
		}

	public:
		AttrStarError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class EmptyExprError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "empty_expr_error" };
		}

	public:
		EmptyExprError(dia::SourcePosition pos): dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadImportChainError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_import_chain_error" };
		}

	public:
		BadImportChainError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	class NoExternArgumentError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_extern_argument_error" };
		}

	public:
		NoExternArgumentError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};

	/**
	 * @brief Error for when a template statement is missing a parameter list.
	 */
	class TemplateNoListError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "template_no_list_error" };
		}

	public:
		TemplateNoListError(dia::SourcePosition pos):
			  dia_int::MessageWithCodeFragmentAndCause(pos) {}
	};
}
