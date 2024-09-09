#include "preamble.hpp"

#include "../../hierarchy/not_statements.hpp"

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

	ParserRef<Attribute> Attribute::parse(RiftParserState& state) {
		auto                 position = state.getPosition();
		ParserRef<Attribute> out      = makeRef<Attribute>(position);

		if (!assertStmtChoice<Attribute>(state, state[0].is(Special::AtSign))) return nullptr;

		state.parse(out).all(Special::AtSign, &out->name);

		// @TODO: Make a more general solution to dotted names that can't have stars
		if (out->name != nullptr && out->name->getStar())
			state.log(base::make_unique<AttrStarError>(out->name->getSourcePosition()));
		if (state[0].isBracketGroup(Token::BracketType::Round)) state.parse(out).one(&out->args);

		return out;
	}

	void Attribute::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\" : ";
		nullAwareDprint(name, out);
		if (args != nullptr) {
			out << ", \"args\": ";
			nullAwareDprint(args, out);
		}
		out << "}";
	}
}
