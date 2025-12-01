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
						.name          = "ambiguous_function_matches" };
		}

	public:
		AmbiguousMatchesError(dia::SourcePosition source_position):
				MessageWithCodeFragmentAndCause(source_position) {}

		void addExploreCoercibleCandidates(usize no_candidates, Box<dia::Message> candidate_list);

		void addExploreFailedCandidates(usize no_candidates, Box<dia::Message> candidate_list);
	};

	Box<dia_int::MessageBase> createDetailedCallErrorMessage(
		query::Context&              ctx,
		SymID                        function_symbol,
		pst::Access<pst::expr::Call> call_expr,
		const MatchFailure&          failure_reason,
		bool                         is_for_candidate_function_msg
	);
}
