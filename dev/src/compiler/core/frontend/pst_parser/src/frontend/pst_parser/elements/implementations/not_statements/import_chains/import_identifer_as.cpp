#include "../../../hierarchy/not_statements/import_chains/import_identifier_as.hpp"
#include "../preamble.hpp"

namespace pst {
	MBox<ImportIdentifierAs> ImportIdentifierAs::parse(LangParserState& state) {
		auto out = makeBox<ImportIdentifierAs>(state.getPosition());

		tpc::Identifier id;

		state.parse(out).all(&id);
		out->names.push_back(id);

		PST_WHILE(state[0].is(NamedOperator::Period)) {
			state.parse(out).all(NamedOperator::Period, &id);
			out->names.push_back(id);
		}

		if (state.parse(out).tryEat(Keyword::As)) {
			state.parse(out).all(&id);
			out->as = id;
		}

		return out;
	}

	void ImportIdentifierAs::dprint(std::ostream& out) const {
		out << "{";

		out << R"("names": [)";
		for (const auto& name: names) {
			nullAwareDprint(name, out);
			out << ",";
		}
		out << "],";

		if (as) {
			out << R"("as": )";
			nullAwareDprint(*as, out);
		}

		out << "}";
	}

	LangElement::HashAlg& ImportIdentifierAs::addElementDataToStableHash(HashAlg& partial_hash
	) const {
		addToHash(partial_hash, names);
		addToHash(partial_hash, as.has_value());
		if (as) addToHash(partial_hash, *as);
		return partial_hash;
	}
}
