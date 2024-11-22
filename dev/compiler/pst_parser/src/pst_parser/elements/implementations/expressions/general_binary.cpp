#include "preamble.hpp"

#include <stack>

namespace pst::expr {
	i64 GeneralBinary::skipLiteral(const LangParserState& state, i64 base, i64 length) {
		i64 fwd = base;
		if (state[fwd].isIdentifier()) { fwd++; }  // Ignores first identifier
		while (fwd < length && !isGenBinOp(state, fwd)
		       && !(state[fwd].isIdentifier() && !state[fwd - 1].is(NamedOperator::Period))) {
			fwd++;
		}
		return fwd;
	}

	ParserRef<ExprElement>
		GeneralBinary::parseRecursive(LangParserState& state, const BuilderExpr& expr) {
		if (std::holds_alternative<i64>(expr)) {
			return Lower::parse(state, std::get<i64>(expr));
		} else {
			auto op  = std::get<base::unique_ptr<OperatorBuilder>>(expr).borrow();
			auto out = base::make_unique<GeneralBinary>(state.getPosition(), op->type);

			state.parse(out).with(&out->left, parseRecursive, op->lhs);
			state.parse(out).one(op->type);
			state.parse(out).with(&out->right, parseRecursive, op->rhs);

			return out;
		}
	}

	ParserRef<ExprElement> GeneralBinary::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing General Binary Expressions" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		i64 fwd            = 0;
		i64 reduced_length = length;
		// Here this should include the prefix word operators in the future
		while (fwd < length && state[fwd].isOperator()) fwd++;
		while (fwd < reduced_length && state[reduced_length - 1].isOperator()) reduced_length--;
		if (fwd == reduced_length) {}  // Error

		std::vector<i64> operators;
		i64              next;
		while (fwd < reduced_length) {
			next = skipLiteral(state, fwd, reduced_length);
			if (fwd == next) {}             // Error
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
		stack.push({ fwd, fwd, getOpPrec(state[fwd].asOperator()) });

		for (i64 i = 1; i < operators.size(); i++) {
			fwd                   = operators[i];
			i64         curr_prec = getOpPrec(state[fwd].asOperator());
			BuilderExpr lhs       = operators[i] - operators[i - 1] - 1;
			while (!stack.empty() && stack.top().op_prec <= curr_prec) {
				Partial partial = std::move(stack.top());
				stack.pop();
				lhs = base::make_unique<OperatorBuilder>(
					std::move(partial.lhs), state[partial.op_place].asOperator(), std::move(lhs)
				);
			}
			stack.push({ std::move(lhs), fwd, curr_prec });
		}

		BuilderExpr rhs = length - operators.back() - 1;
		while (!stack.empty()) {
			Partial partial = std::move(stack.top());
			stack.pop();
			rhs = base::make_unique<OperatorBuilder>(
				std::move(partial.lhs), state[partial.op_place].asOperator(), std::move(rhs)
			);
		}

		return GeneralBinary::parseRecursive(state, rhs);
	}
}
