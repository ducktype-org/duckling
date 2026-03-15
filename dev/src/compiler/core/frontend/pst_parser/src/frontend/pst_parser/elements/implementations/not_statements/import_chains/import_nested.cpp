#include "../../../hierarchy/not_statements/import_chains/import_nested.hpp"

#include "../../../hierarchy/lists/nested_import_list.hpp"  // IWYU pragma: keep
#include "../preamble.hpp"

namespace pst {
	MBox<ImportNested> ImportNested::parse(LangParserState& state) {
		auto out = makeBox<ImportNested>(state);

		tpc::Identifier id;
		PST_WHILE(state[0].isIdentifier()) {
			PARSE().all(&id, NamedOperator::Period);
			out->names.push_back(id);
		}
		PARSE().one(&out->nested_import);

		return out;
	}

	void ImportNested::dprint(std::ostream& out) const {
		out << "{";

		out << R"("names": [)";
		for (const auto& name: names) {
			nullAwareDprint(name, out);
			out << ",";
		}
		out << "],";

		out << R"("nested_import": )";
		nullAwareDprint(nested_import, out);

		out << "}";
	}

	HashAlg& ImportNested::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, names);
		return partial_hash;
	}
}
