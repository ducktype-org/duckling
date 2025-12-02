/**
 * @file errors.hpp
 * @author Wojciech Rzepliński
 * @brief Errors and error messages related to function call processing.
 */
#pragma once

#include "frontend/pst_parser/elements/hierarchy/expressions/call.hpp"
#include "helios/scope_symbol_id.hpp"
#include "typesystem/higher/symbol_type.hpp"

#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/usage.hpp>

#include "diagnostic/source_position.hpp"
#include <diagnostic/message.hpp>

namespace compiler::helios::code {
	/**
	 * @brief These are structs representing specific reasons why a function call matching
	 * could have failed. The contents of these structs will be used to create detailed error
	 * messages.
	 */

	struct PositionalAfterNamedArgument final {
		usize argument_index;
	};

	struct TooManyCallArguments final {
		usize valid_arguments;
		usize total_arguments;
	};

	struct DuplicateNamedArgument final {
		usize argument_index;
	};

	struct UnknownNamedArgument final {
		base::StrID name;
		usize       argument_index;
	};

	struct TypeMismatch final {
		tsh::SymbolType<> given_type;
		tsh::SymbolType<> expected_type;
		usize             argument_index;
	};

	struct MissingCallArgument final {
		/** Index of the parameter of the declaration that was not filled */
		usize parameter_index;
	};

	using MatchFailure = std::variant<
		TooManyCallArguments,
		DuplicateNamedArgument,
		UnknownNamedArgument,
		TypeMismatch,
		MissingCallArgument,
		PositionalAfterNamedArgument>;

	class AmbiguousMatchesError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "call_ambiguous_matches" };
		}

	public:
		AmbiguousMatchesError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}

		// void addExplore

		void addExploreCoercibleCandidates(
			usize no_candidates, Box<dia_int::MessageBase> candidate_list
		);

		void addExploreFailedCandidates(
			usize no_candidates, Box<dia_int::MessageBase> candidate_list
		);

		void addExploreExactCandidates(usize no_candidates, Box<dia_int::MessageBase> candidate_list);
	};

	Box<dia_int::MessageBase> createDetailedCallErrorMessage(
		query::Context&              ctx,
		SymID                        function_symbol,
		pst::Access<pst::expr::Call> call_expr,
		const MatchFailure&          failure_reason,
		bool                         is_for_candidate_function_msg
	);

	pst::Access<pst::ParamList> getFunctionParamList(
		query::Context& ctx, pst::Access<pst::LangElement> function_decl
	);

	pst::Access<pst::LangElement> getNthCallArgument(
		query::Context& ctx, pst::Access<pst::expr::Call> call_expr, usize argument_index
	);

	pst::Access<pst::LangElement> getNthDeclarationParameter(
		query::Context& ctx, pst::Access<pst::LangElement> function_decl, usize parameter_index
	);

	class CoercibleCandidateCoercionPointerMessage final: public dia_int::MessageBase {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "pointer_message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "coercible_candidate_coercion_pm" };
		}

	public:
		CoercibleCandidateCoercionPointerMessage(
			std::string parameter_type, std::string argument_type
		):
			  MessageBase() {
			addArgument<dia_int::TextArgument>("parameter_type", std::move(parameter_type));
			addArgument<dia_int::TextArgument>("argument_type", std::move(argument_type));
		}
	};

	class CoercibleCandidateNote final: public dia_int::MessageWithCodeFragment {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "coercible_candidate" };
		}

	public:
		CoercibleCandidateNote(dia::SourcePosition source_position):
			  MessageWithCodeFragment(source_position) {}
	};

	class ExactCandidateNote final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "exact_candidate" };
		}

	public:
		ExactCandidateNote(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};

	class FailedCandidateNote final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "note",
				     .family        = "type_check",
				     .name          = "failed_candidate" };
		}

	public:
		FailedCandidateNote(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
