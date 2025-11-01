/**
 * @file errors.hpp
 * @author Wojciech Rzepliński
 * @brief Errors and error messages related to function call processing.
 */


#include <diagnostic/message.hpp>

namespace compiler::helios::code {

	struct ToManyCallArguments {};

	struct DuplicateNamedArgument {};

	struct UnknownNamedArgument {};

	struct TypeMismatch {};

	struct MissingCallArgument {};

	struct PositionalAfterNamedArgument {};

	struct RepeatedNamedArgument {};

	using MatchFailure = std::variant<
		ToManyCallArguments,
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

}
