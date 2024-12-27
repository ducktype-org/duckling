#include "preamble.hpp"

namespace pst {
	class NoSpecifierError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Expected an access specifier.";
		}

	public:
		NoSpecifierError(dia::SourcePosition pos): dia::Error(pos) {}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}
	};

	MBox<AccessBlock> AccessBlock::parse(LangParserState& state, const ClassContext& ctx) {
		auto position = state.getPosition();
		auto out      = makeBox<AccessBlock>(position, ctx);

		if (not access_specifiers.contains(state[0].asKeyword())) {
			state.log(base::make_unique<NoSpecifierError>(position));
		} else {
			out->context.specifiers.emplace_back(&state[0]);
			out->specifier = state[0].asKeyword();
		}

		// Consume the keyword specifying the visibility
		state.parse(out).eatOne();

		state.parse(out).with(&out->block, ClassBlock::parse, out->getContext());

		return out;
	}

	void AccessBlock::dprint(std::ostream& out) const {
		out << "{";

		out << R"("specifier": )";
		tpc::nullAwareDprint(specifier, out);
		out << R"(, "code block": )";
		nullAwareDprint(block, out);

		out << "}";
	}

	void AccessBlock::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitAccessBlock(*this);
	}
}
