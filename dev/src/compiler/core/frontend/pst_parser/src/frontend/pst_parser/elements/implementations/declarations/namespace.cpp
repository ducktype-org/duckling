#include "../../hierarchy/declarations/namespace.hpp"

#include "../../hierarchy/not_statements/code_block.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	MBox<Namespace> Namespace::parse(LangParserState& state) {
		auto out = makeBox<Namespace>(state);

		if (!assertStmtChoice<Namespace>(state, state[0].is(Keyword::Namespace))) return nullptr;

		PST_NEW_CONTEXT({
			state.setContextBlockOrdering(BlockOrderType::Unordered);
			PARSE().all(Keyword::Namespace, &out->name, &out->body);
		})

		PST_RETURN out;
	}

	void Namespace::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);

		// @TODO: change to body in print:
		out << R"(, "block": )";
		nullAwareDprint(body, out);

		out << "}";
	}

	HashAlg& Namespace::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void Namespace::acceptVisitor(PstVisitor& visitor) const { visitor.visitNamespace(*this); }
}
