/**
 * @file errors.hpp
 * @author Wojciech Rzepliński
 * @brief Errors and error messages related to function call processing.
 */


#include "string_id/string_id.hpp"
#include <diagnostic/message.hpp>

namespace compiler::helios::code {
	/**
	 * @brief These are structs representing specific reasons why a function call matching
	 * could have failed. The contents of these structs will be used to create detailed error messages.
	 */

	struct PositionalAfterNamedArgument final {
		usize argument_index;
	};
	
	struct TooManyCallArguments final {
		usize last_valid_argument;
	};

	struct DuplicateNamedArgument final {
		usize index_in_named_list;
	};

	struct UnknownNamedArgument final {
		base::StrID argument;
	};

	struct TypeMismatch final {
		usize parameter_index;
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
		MissingCallArgument>;

	class AmbiguousExactMatches final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Ambiguous exact matches found during function overloading.";
		}

	public:
		[[nodiscard]] Domain getDomain() const override { return Domain::TypeCheck; }

		explicit AmbiguousExactMatches(const dia::SourcePosition& source_position):
			  Error(source_position) {}
	};

	class AmbiguousCoercionMatches final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Ambiguous matches requiring argument coercions during function overloading.";
		}

	public:
		[[nodiscard]] Domain getDomain() const override { return Domain::TypeCheck; }

		explicit AmbiguousCoercionMatches(const dia::SourcePosition& source_position):
			  Error(source_position) {}
	};

	class InvalidCallExpression final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Call expression doesn't match the signature of any function.";
		}

	public:
		[[nodiscard]] Domain getDomain() const override { return Domain::TypeCheck; }

		explicit InvalidCallExpression(const dia::SourcePosition& source_position):
			  Error(source_position) {}
	};

	/**
	 * If there was more than one exact matches we always attach the both to the displayed error,
	 * but we add a coercion matches and failed matches as explore links.

	 * If there was more than one coercion matches we always attach the both to the displayed error,
	 * but we add failed matches as explore links.

	 * If there was no matches we always attach the failed matches.
	 * The error is like:
	 * Failed to call candidate function:

	   Failed to call function/method <name>.
	   <reason for failure>

	   note: Candidate function/method <name>
	   <snippet of declaration>
	   error: <reason for failure>
	 */
}
