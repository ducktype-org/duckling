#include "preamble.hpp"

namespace pst {
	class NonEmptyError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected no arguments for destructor.";
		}

	public:
		NonEmptyError(dia::SourcePosition pos): dia::Error(pos) {}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}
	};

	ParserRef<Destructor> Destructor::parse(RiftParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeRef<Destructor>(position, ctx);

		out->parseSpecifiers(state);

		state.parse(out).eatOne();
		state.parse(out).all(Operator::Period, &out->kind);

		state.parse(out).goDown();
		if (state.notEmpty()) state.log(base::make_unique<NonEmptyError>(state.getPosition()));
		state.parse(out).goUpAndSkip();

		state.parse(out).one(&out->body);

		return out;
	}

	void Destructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Destructor::acceptVisitor(PstStmtVisitor& visitor) const {
		visitor.visitDestructor(*this);
	}
}
