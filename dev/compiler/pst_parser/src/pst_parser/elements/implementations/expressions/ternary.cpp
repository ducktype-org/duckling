#include "preamble.hpp"

namespace pst::expr {
	class MultipleTernaryError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Multiple repeating ternary components in a single expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		MultipleTernaryError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class PartialTernaryError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Partial ternary expression";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		PartialTernaryError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	class ImproperTernaryError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Ternary expression starting in an improper place";
		}

	public:
		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}

		ImproperTernaryError(dia::SourcePosition pos): dia::Error(pos) {}
	};

	MBox<ExprElement> Ternary::parse(LangParserState& state, i64 length) {
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
					state.log(makeBox<MultipleTernaryError>(pos));
					fastForward(state, length);
					return nullptr;
				}
				if (i != 0) {
					state.log(makeBox<ImproperTernaryError>(pos));
					fastForward(state, length);
					return nullptr;
				}
				if (!if_found) if_found = true;
			} else if (state[i].is(Keyword::Then)) {
				if (!if_found) {
					state.log(makeBox<PartialTernaryError>(pos));
					fastForward(state, length);
					return nullptr;
				}
				if (then_found) {
					state.log(makeBox<MultipleTernaryError>(pos));
					fastForward(state, length);
					return nullptr;
				}
				if (!then_found) {
					then_found = true;
					then_fwd   = i;
				}
			} else if (state[i].is(Keyword::Else)) {
				if (!if_found || !then_found) {
					state.log(makeBox<PartialTernaryError>(pos));
					fastForward(state, length);
					return nullptr;
				}
				if (else_found) {
					state.log(makeBox<MultipleTernaryError>(pos));
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
			state.log(makeBox<PartialTernaryError>(pos));
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

	void Ternary::acceptExprVisitor(PstExprVisitor& visitor) const { visitor.visitTernary(*this); }
}
