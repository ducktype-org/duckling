#include "../../hierarchy/expressions/general_binary.hpp"

#include "../../hierarchy/expressions/general_suffix.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

#include <stack>

namespace pst::expr {
	class OnlyPrefixError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expression has only prefix operators";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		OnlyPrefixError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	i64 GeneralBinary::skipAtom(const LangParserState& state, i64 base, i64 length) {
		i64 fwd = base;
		if (state[fwd].isIdentifier()) { fwd++; }  // Ignores first identifier
		while (fwd < length
		       && !(
				   state[fwd].isOperatorSymbol()
				   && state[fwd].asBinaryOperator().value().isNotReserved()
			   )
		       && !(
				   state[fwd].isIdentifier()
				   && !state[fwd - 1].asBinaryOperator().map([](auto x) { return x.isAccessOp(); }
		           ).copyValueOr(false)
			   )) {
			fwd++;
		}
		return fwd;
	}

	/**
	 * @todo Improve error reporting/strategy here If something isn't completely parsed some weird
	 * errors might occur: fun foo() -> i64 {} fun foo2() -> i64 = {}
	 */
	MBox<ExprElement> GeneralBinary::parseRecursive(LangParserState& state, const BuilderExpr& expr) {
		if (std::holds_alternative<i64>(expr)) {
			return Lower::parse(state, std::get<i64>(expr));
		} else {
			auto op  = std::get<Box<OperatorBuilder>>(expr).ref();
			auto out = makeBox<GeneralBinary>(state.getPosition(), op->type);

			state.parse(out).with(&out->left, parseRecursive, op->lhs);
			state.parse(out).one(op->type);
			state.parse(out).with(&out->right, parseRecursive, op->rhs);

			return out;
		}
	}

	MBox<ExprElement> GeneralBinary::parse(LangParserState& state, i64 length) {
		if (!checkLength(state, length)) return nullptr;

		auto pos
			= dia::SourcePosition(state.getPosition(), state.getPosition((i64) length - 1).getEnd());

		i64 fwd            = 0;
		i64 reduced_length = length;
		// Here this should include the prefix word operators in the future
		while (fwd < length && state[fwd].isPrefixOperator()) fwd++;
		while (fwd < reduced_length && state[reduced_length - 1].isOperatorSymbol())
			reduced_length--;
		if (fwd == reduced_length) state.log(makeBox<OnlyPrefixError>(pos));

		std::vector<i64> operators;
		i64              next = 0;
		while (fwd < reduced_length) {
			next = skipAtom(state, fwd, reduced_length);
			if (fwd == next) makeBox<tpc::NoIdentifierError>(state.getPosition(fwd));
			if (next < reduced_length - 1)  // Not a suffix operator or end of expression
				operators.push_back(next);
			fwd = std::min(next + 1, reduced_length);
		}

		if (operators.size() == 0) return Lower::parse(state, length);

		struct Partial {
			BuilderExpr lhs;
			i64         op_place;
			i64         op_prec;
		};

		std::stack<Partial> stack;
		fwd = operators[0];
		stack.push({ fwd, fwd, state[fwd].asBinaryOperator().value().getGenBinOpPrecedence() });

		for (u64 i = 1; i < operators.size(); i++) {
			fwd                   = operators[i];
			i64         curr_prec = state[fwd].asBinaryOperator().value().getGenBinOpPrecedence();
			BuilderExpr lhs       = operators[i] - operators[i - 1] - 1;
			while (!stack.empty() && stack.top().op_prec <= curr_prec) {
				Partial partial = std::move(stack.top());
				stack.pop();
				lhs = makeBox<OperatorBuilder>(
					std::move(partial.lhs),
					state[partial.op_place].asBinaryOperator().value(),
					std::move(lhs)
				);
			}
			stack.push({ std::move(lhs), fwd, curr_prec });
		}

		BuilderExpr rhs = length - operators.back() - 1;
		while (!stack.empty()) {
			Partial partial = std::move(stack.top());
			stack.pop();
			rhs = makeBox<OperatorBuilder>(
				std::move(partial.lhs),
				state[partial.op_place].asBinaryOperator().value(),
				std::move(rhs)
			);
		}

		return GeneralBinary::parseRecursive(state, rhs);
	}
}
