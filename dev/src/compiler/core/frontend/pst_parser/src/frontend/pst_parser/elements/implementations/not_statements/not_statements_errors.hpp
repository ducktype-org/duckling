// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/message.hpp>

namespace pst {
	class UnrecognizedPatternInCaseError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "unrecognized_pattern_in_case_error" };
		}

	public:
		UnrecognizedPatternInCaseError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class RoundExprStartError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "round_expr_start_error" };
		}

	public:
		RoundExprStartError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MatchCaseWithNoBodyError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "match_case_no_body" };
		}

	public:
		MatchCaseWithNoBodyError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class DoubleDefaultBranchError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "double_default_branch" };
		}

	public:
		DoubleDefaultBranchError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class UnconditionedBranchAfterConditionedError final:
		  public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "unconditioned_branch_after_conditioned" };
		}

	public:
		UnconditionedBranchAfterConditionedError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class AttrStarError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "attr_star_error" };
		}

	public:
		AttrStarError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class EmptyExprError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "empty_expr_error" };
		}

	public:
		EmptyExprError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadSelectorError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_selector_error" };
		}

	public:
		BadSelectorError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class NoExternArgumentError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_extern_argument_error" };
		}

	public:
		NoExternArgumentError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	/**
	 * @brief Error for when a template statement is missing a parameter list.
	 */
	class TemplateNoListError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "template_no_list_error" };
		}

	public:
		TemplateNoListError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	/**
	 * @brief Error for operators that cannot be used as function names.
	 */
	class ReservedOperatorFunNameError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "reserved_operator_fun_name" };
		}

	public:
		ReservedOperatorFunNameError(dia::SourcePosition pos, std::string op):
			  dia::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("operator", std::move(op));
		}
	};

	/**
	 * @brief Error for custom assignment operators, which are not supported yet.
	 */
	class AssignmentOperatorFunNameError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "assignment_operator_fun_name" };
		}

	public:
		AssignmentOperatorFunNameError(dia::SourcePosition pos, std::string op):
			  dia::MessageWithCodeFragmentAndCause(pos) {
			addArgument<dia::TextArgument>("operator", std::move(op));
		}
	};
}
