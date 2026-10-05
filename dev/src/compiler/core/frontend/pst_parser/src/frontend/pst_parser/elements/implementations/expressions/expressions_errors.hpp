// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <diagnostic/message.hpp>

namespace pst {
	/**
	 * @brief For now this is a safety error (meaning it should never happen), unless there will be
	 * some situation where only a number value will be accepted in an expression.
	 */
	class BadValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_value_error" };
		}

	public:
		BadValueError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MoreThanValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "more_than_value_error" };
		}

	public:
		MoreThanValueError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadUnitExprError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_unit_expr_error" };
		}

	public:
		BadUnitExprError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MultipleTernaryError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "multiple_ternary" };
		}

	public:
		MultipleTernaryError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class PartialTernaryError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "partial_ternary" };
		}

	public:
		PartialTernaryError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class ImproperTernaryError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "improper_ternary" };
		}

	public:
		ImproperTernaryError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	/**
	 * @brief For now this is a safety error (meaning it should never happen), unless there will be
	 * some situation where only a string value will be accepted in an expression.
	 */
	class BadStrValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_str_value_error" };
		}

	public:
		BadStrValueError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MoreThanStrValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "more_than_str_value_error" };
		}

	public:
		MoreThanStrValueError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadRoundExprError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_round_expr_error" };
		}

	public:
		BadRoundExprError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MatchRoundBracketError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "match_round_bracket_error" };
		}

	public:
		MatchRoundBracketError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class NotACaseExpression final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "not_a_case_expression" };
		}

	public:
		NotACaseExpression(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MatchCurlyBracketError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "match_curly_bracket_error" };
		}

	public:
		MatchCurlyBracketError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class OnlyPrefixError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "only_prefix_error" };
		}

	public:
		OnlyPrefixError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	/**
	 * @brief For now this is a safety error (meaning it should never happen), unless there will be
	 * some situation where only a string value will be accepted in an expression.
	 */
	class BadCharValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_char_value_error" };
		}

	public:
		BadCharValueError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MoreThanCharValueError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "more_than_char_value_error" };
		}

	public:
		MoreThanCharValueError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadChainExprError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_chain_expr_error" };
		}

	public:
		BadChainExprError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadCallError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_call_error" };
		}

	public:
		BadCallError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadBlockError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_block_error" };
		}

	public:
		BadBlockError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class NoAtomError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "no_atom_error" };
		}

	public:
		NoAtomError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class MultipleAssignmentError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "multiple_assignment_error" };
		}

	public:
		MultipleAssignmentError(dia::SourcePosition pos):
			  dia::MessageWithCodeFragmentAndCause(pos) {}
	};

	class BadAccessError final: public dia::MessageWithCodeFragmentAndCause {
		dia::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "parser",
				     .name          = "bad_access_error" };
		}

	public:
		BadAccessError(dia::SourcePosition pos): dia::MessageWithCodeFragmentAndCause(pos) {}
	};

}
