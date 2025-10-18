#include "../../hierarchy/expressions/value.hpp"

#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief For now this is a safety error (meaning it should never happen), unless there will be
	 * some situation where only a number value will be accepted in an expression.
	 */
	class BadValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected a value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		BadValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class MoreThanValueError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected just a single value";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MoreThanValueError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> ExprValue::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		if (!state[0].is(lexer::Token::Type::NumLiteral)) {
			// This should (probably) never happen with how it's called by the parser
			state.log(makeBox<BadValueError>(pos));
			fastForward(state, length);
			return nullptr;
		}

		const auto&                 token = state[0];
		base::StrID                 value;
		base::Optional<base::StrID> type_specifier;


		if (!token.getRecursive().empty()) {  // Type specifier exists.
			const auto& sub_tokens = token.getRecursive();
			CORE_ASSERT(
				sub_tokens.size() == 2, "Complex numeric literals should have two subtokens"
			);
			const auto& value_token     = sub_tokens[0];
			const auto& specifier_token = sub_tokens[1];
			CORE_ASSERT(value_token.isNumLiteral(), "First sub-token must be a numLiteral");
			CORE_ASSERT(
				specifier_token.isTypeSpecifier(), "Second sub-token must be a typeSpecifier"
			);

			value          = sub_tokens[0].getValue();
			type_specifier = sub_tokens[1].getValue();
		} else {
			value          = token.getValue();
			type_specifier = {};
		}


		auto out = makeBox<ExprValue>(pos, value, type_specifier.map([](const base::StrID& val) {
			return lexer::Value(val);
		}));
		state.parse(out).eatOne();

		if (length > 1) {
			state.log(makeBox<MoreThanValueError>(pos));
			fastForward(state, length);
		}

		return out;
	}

	void ExprValue::dprint(std::ostream& out) const {
		out << "{";

		out << R"("number": ")" << number.str() << "\"";

		if (type_specifier) out << R"(, "type_specifier": ")" << type_specifier->str() << "\"";

		out << "}";
	}

	LangElement::HashAlg& ExprValue::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, number);
		if (type_specifier) addToHash(partial_hash, *type_specifier);
		return partial_hash;
	}

	void ExprValue::acceptExprVisitor(PstExprVisitor& visitor) const {
		visitor.visitExprValue(*this);
	}
}
