#include "../../../hierarchy/not_statements/import_chains/import_identifier_as.hpp"
#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ImportIdentifierAs, names, as);

	MBox<ImportIdentifierAs> ImportIdentifierAs::parse(LangParserState& state) {
		auto out = makeBox<ImportIdentifierAs>(state);

		MBox<IdentifierWrapper> id;

		auto push_id = [&]() {
			out->names.emplace_back(nullptr);
			PARSE().assign(&out->names.back(), std::move(id));
		};

		PARSE().all(&id);
		push_id();

		PST_WHILE(state[0].is(NamedOperator::Period)) {
			PARSE().all(NamedOperator::Period, &id);
			push_id();
		}

		if (PARSE().tryEat(NamedOperator::As)) {
			PARSE().all(&id);
			PARSE().assign(&out->as, std::move(id));
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

	HashAlg& ImportIdentifierAs::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, names.size());
		hashing::addToHash(partial_hash, as.has_value());
		return partial_hash;
	}

	void ImportIdentifierAs::calcElementPathHashRecursive() {
		calcIndexedListChildPath<IdentifierWrapper>({ names }, getElementPathHash());
		if (as.has_value()) calcNamedChildPath(as.value(), getElementPathHash());
	}
}
