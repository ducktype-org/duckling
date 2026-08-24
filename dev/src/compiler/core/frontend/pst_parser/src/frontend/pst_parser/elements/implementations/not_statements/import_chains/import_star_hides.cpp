#include "../../../hierarchy/not_statements/import_chains/import_star_hides.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(ImportStarHides, names, hides);

	MBox<ImportStarHides> ImportStarHides::parse(LangParserState& state) {
		auto out = makeBox<ImportStarHides>(state);

		MBox<IdentifierWrapper> id;

		auto push_id = [&](auto& id_list) {
			id_list.emplace_back(nullptr);
			PARSE().assign(&id_list.back(), std::move(id));
		};

		PARSE().one(&id);
		push_id(out->names);

		PST_WHILE(state[0].is(NamedOperator::Period)) {
			PARSE().all(NamedOperator::Period, &id);
			push_id(out->names);
		}

		PARSE().one(NamedOperator::PeriodStar);

		if (PARSE().tryEat(Keyword::Hides)) {
			out->hides.emplace();

			PARSE().all(&id);
			push_id(out->hides.value());

			PST_WHILE(state[0].is(Special::Comma)) {
				PARSE().all(Special::Comma, &id);
				push_id(out->hides.value());
			}
		}

		return out;
	}

	void ImportStarHides::dprint(std::ostream& out) const {
		out << "{";

		out << R"("names": [)";
		for (const auto& name: names) {
			nullAwareDprint(name, out);
			out << ",";
		}
		out << "],";

		if (hides) {
			out << R"("hides": [)";
			for (const auto& name: *hides) {
				nullAwareDprint(name, out);
				out << ",";
			}
			out << "],";
		}

		out << "}";
	}

	HashAlg& ImportStarHides::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, names.size());
		hashing::addToHash(partial_hash, hides.has_value());
		if (hides) hashing::addToHash(partial_hash, hides->size());
		return partial_hash;
	}

	void ImportStarHides::calcElementPathHashRecursive() {
		calcIndexedListChildPath<IdentifierWrapper>({ names }, { getElementPathHash(), "names" });
		if (hides.has_value())
			calcIndexedListChildPath<IdentifierWrapper>(
				{ hides.value() }, { getElementPathHash(), "hides" }
			);
	}
}
