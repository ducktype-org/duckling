#include "../../hierarchy/expressions/ternary.hpp"

#include "../../hierarchy/expressions/logic_or.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst::expr {

	MBox<ExprElement> Ternary::parse(LangParserState& state) {
		if (!checkNonEmpty(state)) return nullptr;

		i64 length = base::safeIntConv<i64>(state.ctokens().size());

		auto pos = dia::SourcePosition(state.getPosition(), state.getPosition(length - 1).getEnd());

		bool if_found   = false;
		bool then_found = false;
		i64  then_fwd   = 0;
		bool else_found = false;
		i64  else_fwd   = 0;

		for (i64 i = 0; i < length; i++) {
			if (state[i].is(Keyword::If)) {
				if (if_found) {
					state.logInt(makeBox<MultipleTernaryError>(pos));
					return nullptr;
				}
				if (i != 0) {
					state.logInt(makeBox<ImproperTernaryError>(pos));
					return nullptr;
				}
				if (!if_found) if_found = true;
			} else if (state[i].is(Keyword::Then)) {
				if (!if_found) {
					state.logInt(makeBox<PartialTernaryError>(pos));
					return nullptr;
				}
				if (then_found) {
					state.logInt(makeBox<MultipleTernaryError>(pos));
					return nullptr;
				}
				if (!then_found) {
					then_found = true;
					then_fwd   = i;
				}
			} else if (state[i].is(Keyword::Else)) {
				if (!if_found || !then_found) {
					state.logInt(makeBox<PartialTernaryError>(pos));
					return nullptr;
				}
				if (else_found) {
					state.logInt(makeBox<MultipleTernaryError>(pos));
					return nullptr;
				}
				if (!else_found) {
					else_found = true;
					else_fwd   = i;
				}
			}
		}
		if (!if_found) return Lower::parse(state);
		if (if_found && !else_found) {
			state.logInt(makeBox<PartialTernaryError>(pos));
			return nullptr;
		}

		auto out = makeBox<Ternary>(state);

		PARSE().one(Keyword::If);
		PARSE().autoFallbackLen(then_fwd - 1).with(&out->condition, Lower::parse);

		PARSE().one(Keyword::Then);
		PARSE().autoFallbackLen(else_fwd - then_fwd - 1).with(&out->if_true, Lower::parse);

		PARSE().one(Keyword::Else);
		PARSE().autoFallbackLen(length - else_fwd - 1).with(&out->if_false, Lower::parse);

		PST_RETURN out;
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

	HashAlg& Ternary::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Ternary::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitTernary(*this); }

	AccessLocked<ExprElement> Ternary::getCondition() const { return condition.give(); }

	AccessLocked<ExprElement> Ternary::getIfTrue() const { return if_true.give(); }

	AccessLocked<ExprElement> Ternary::getIfFalse() const { return if_false.give(); }
}
