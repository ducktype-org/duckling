#include "../../hierarchy/not_statements/attribute.hpp"

#include "preamble.hpp"

namespace base::extend {
	void BoxPtrDeleter<pst::Attribute>::del(pst::Attribute* ptr) { delete ptr; }
}

namespace pst {
	class AttrStarError final: public dia::Error {
	protected:
		[[nodiscard]]
		std::string toStringBrief() const override {
			return "Unexpected `.*` in Attribute name";
		}

	public:
		AttrStarError(dia::SourcePosition pos): dia::Error(pos) {}

		[[nodiscard]]
		Domain getDomain() const override {
			return Domain::Parser;
		}
	};

	MBox<Attribute> Attribute::parse(LangParserState& state) {
		auto           position = state.getPosition();
		Box<Attribute> out      = makeBox<Attribute>(position);

		if (!assertStmtChoice<Attribute>(state, state[0].is(Special::AtSign))) return nullptr;

		state.parse(out).all(Special::AtSign, &out->name);

		// @TODO: Make a more general solution to dotted names that can't have stars
		if (out->name.internal() && out->name.internal()->getStar())
			state.log(makeBox<AttrStarError>(out->name.internal()->getSourcePosition()));
		if (state[0].isBracketGroup(Token::BracketType::Round)) state.parse(out).one(&out->args);

		return out;
	}

	void Attribute::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\" : ";
		nullAwareDprint(name, out);
		if (args.internal()) {
			out << ", \"args\": ";
			nullAwareDprint(args, out);
		}
		out << "}";
	}
}
