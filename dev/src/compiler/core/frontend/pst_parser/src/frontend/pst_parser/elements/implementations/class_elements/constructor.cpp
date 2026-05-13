#include "../../hierarchy/class_elements/constructor.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "../../hierarchy/not_statements/param.hpp"       // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Constructor> Constructor::parse(LangParserState& state) {
		auto out = makeBox<Constructor>(state);

		PARSE().eatOne();

		if (!state[0].isBracketGroup(Token::Round)) {
			MBox<IdentifierWrapper> ident;
			PARSE().all(NamedOperator::Period, &ident);
			out->ident.emplace();
			PARSE().assign(&out->ident, std::move(ident));
		}

		PARSE().one(&out->params);
		if (PARSE().tryEat(NamedOperator::Colon)) PARSE().one(&out->inits);

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Ordered);
			PARSE().all(NamedOperator::Assign, &out->body);
		})

		PST_RETURN out;
	}

	void Constructor::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\":";
		out << "\"" << getInternalSymbolName().value().str() << "\"";
		out << ",\"params\":";
		nullAwareDprint(params, out);
		out << ",\"inits\":";
		nullAwareDprint(inits, out);
		out << ",\"body\":";
		nullAwareDprint(body, out);
		out << "}";
	}

	void Constructor::acceptVisitor(PstVisitor& visitor) const { visitor.visitConstructor(*this); }
}
