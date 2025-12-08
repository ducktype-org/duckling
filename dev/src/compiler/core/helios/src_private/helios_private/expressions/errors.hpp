/**
 * @file errors.hpp
 * @brief Errors and error messages related to helios expression processing.
 */
#include <diagnostic/message.hpp>

#include <string>

namespace compiler::helios::code {
	class UndefinedBinaryOperator final: public dia::Error {
	private:
		std::string operator_symbol;
		std::string lhs_type;
		std::string rhs_type;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "No builtin binary operator '" + operator_symbol + "' matches operands of type '"
			     + lhs_type + "' and '" + rhs_type + "'.";
		}

	public:
		[[nodiscard]] Domain getDomain() const override { return Domain::TypeCheck; }

		explicit UndefinedBinaryOperator(
			const dia::SourcePosition& source_position,
			std::string                op,
			std::string                lhs_t,
			std::string                rhs_t
		):
			  Error(source_position),
			  operator_symbol(std::move(op)),
			  lhs_type(std::move(lhs_t)),
			  rhs_type(std::move(rhs_t)) {}
	};

	class UndefinedUnaryOperator final: public dia::Error {
	private:
		std::string operator_symbol;
		std::string operand_type;

	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "No builtin unary operator '" + operator_symbol + "' matches operand of type '"
			     + operand_type + "'.";
		}

	public:
		[[nodiscard]] Domain getDomain() const override { return Domain::TypeCheck; }

		explicit UndefinedUnaryOperator(
			const dia::SourcePosition& source_position, std::string op, std::string operand_type
		):
			  Error(source_position),
			  operator_symbol(std::move(op)),
			  operand_type(std::move(operand_type))

		{}
	};

}
