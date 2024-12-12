#include "preamble.hpp"

namespace pst::expr {
	MBox<ExprElement> Ternary::parse(LangParserState& state, i64 length) {
		// std::cerr << "Parsing Ternary" << std::endl;
		if (!checkLength(state, length)) return nullptr;

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		bool if_found   = false;
		bool then_found = false;
		i64  then_fwd   = 0;
		bool else_found = false;
		i64  else_fwd   = 0;

		for (i64 i = 0; i < length; i++) {
			if (state[i].is(Keyword::If)) {
				if (if_found) {
					std::cerr << "Ternary error 1\n";
					// Partial ternary expression error
					fastForward(state, length);
					return nullptr;
				}
				if (i != 0) {
					std::cerr << "Ternary error 2\n";
					// ternary expression in improper context error
					fastForward(state, length);
					return nullptr;
				}
				if (!if_found) if_found = true;
			} else if (state[i].is(Keyword::Then)) {
				if (!if_found) {
					std::cerr << "Ternary error 3\n";
					// Partial ternary expression error
					fastForward(state, length);
					return nullptr;
				}
				if (then_found) {
					std::cerr << "Ternary error 4\n";
					// multiple ternary in one expression error
					fastForward(state, length);
					return nullptr;
				}
				if (!then_found) {
					then_found = true;
					then_fwd   = i;
				}
			} else if (state[i].is(Keyword::Else)) {
				if (!if_found || !then_found) {
					std::cerr << "Ternary error 5\n";
					// Partial ternary expression error
					fastForward(state, length);
					return nullptr;
				}
				if (else_found) {
					std::cerr << "Ternary error 6\n";
					// multiple ternary in one expression error
					fastForward(state, length);
					return nullptr;
				}
				if (!else_found) {
					else_found = true;
					else_fwd   = i;
				}
			}
		}
		if (!if_found) return Lower::parse(state, length);
		if (if_found && !else_found) {
			std::cerr << "Ternary error 7\n";
			// Partial ternary expression error
			fastForward(state, length);
			return nullptr;
		}

		auto out = makeBox<Ternary>(pos);

		state.parse(out).one(Keyword::If);
		state.parse(out).with(&out->condition, Lower::parse, then_fwd - 1);

		state.parse(out).one(Keyword::Then);
		state.parse(out).with(&out->if_true, Lower::parse, else_fwd - then_fwd - 1);

		state.parse(out).one(Keyword::Else);
		state.parse(out).with(&out->if_false, Lower::parse, length - else_fwd - 1);
		return out;
	}

	void Ternary::dprint(std::ostream& out) const {
		out << "{";

		out << R"("condition": )";
		nullAwareDprint(condition, out);
		out << R"(, "if_true": )";
		nullAwareDprint(condition, out);
		out << R"(, "if_false": )";
		nullAwareDprint(condition, out);

		out << "}";
	}

	void Ternary::acceptVisitor(PstExprVisitor& visitor) const { visitor.visitTernary(*this); }
}
