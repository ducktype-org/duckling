/**
 * @file errors.hpp
 * @author Wojciech Rzepliński
 * @brief Errors and error messages related to function call processing.
 */
#pragma once

#include <diagnostic_interactive/message.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/call.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/symbol_type.hpp>

#include <diagnostic/source_position.hpp>

namespace compiler::helios::code {
	/**
	 * @brief These are structs representing specific reasons why a function call matching
	 * could have failed. The contents of these structs will be used to create detailed error
	 * messages.
	 */


	struct RepeatedNamedArgument final {
		usize argument_index;
	};

	struct PositionalAfterNamedArgument final {
		usize argument_index;
	};

	struct TooManyCallArguments final {
		usize valid_arguments;
		usize total_arguments;
		SymID function;
	};

	struct NamedArgumentProvidedByPositional final {
		usize argument_index;
		SymID function;
	};

	struct UnknownNamedArgument final {
		base::StrID name;
		usize       argument_index;
		SymID       function;
	};

	struct TypeMismatch final {
		tsh::SymbolType<> given_type;
		tsh::SymbolType<> expected_type;
		usize             argument_index;
		SymID             function;
	};

	struct MissingCallArgument final {
		/** Index of the parameter of the declaration that was not filled */
		usize parameter_index;
		SymID function;
	};

	struct TypeNotTriviallyCopyable final {
		usize             argument_index;
		tsh::SymbolType<> given_type;
		tsh::SymbolType<> expected_type;
		SymID             function;
	};

	using FunctionMatchFailure = std::variant<
		TooManyCallArguments,
		NamedArgumentProvidedByPositional,
		UnknownNamedArgument,
		TypeMismatch,
		MissingCallArgument,
		TypeNotTriviallyCopyable>;

	/**
	 * @brief This variant stores errors that do not depend on the function declaration. All
	 * failures depending on the declaration should be stored in `FunctionMatchFailure`.
	 */
	using CallFailure
		= std::variant<PositionalAfterNamedArgument, RepeatedNamedArgument, FunctionMatchFailure>;

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

	/**
	 * @brief Creates a call error message based on the provided failure reason.
	 * @param ctx The query context.
	 * @param whole_call_origin The ElementOrigin of the entire call expression.
	 * @param arguments_origin The ElementOrigins of all arguments of the call expression.
	 * @param failure_reason The reason for the call failure.
	 * @param is_for_candidate_function Whether the message is for a candidate function
	 * (used in ambiguous matches) or for the main call error.
	 * @return A detailed error message describing the call failure.
	 */
	Box<dia_int::MessageBase> createDetailedCallErrorMessage(
		query::Context&            ctx,
		ElementOrigin              whole_call_origin,
		std::vector<ElementOrigin> arguments_origin,
		const CallFailure&         failure_reason,
		bool                       is_for_candidate_function
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

	class NoCandidatesFoundError final: public dia_int::MessageWithCodeFragmentAndCause {
		dia_int::Metadata getMetadata() const final {
			return { .template_type = "message",
				     .type          = "error",
				     .family        = "type_check",
				     .name          = "no_candidates_found" };
		}

	public:
		NoCandidatesFoundError(dia::SourcePosition source_position):
			  MessageWithCodeFragmentAndCause(source_position) {}
	};
}
