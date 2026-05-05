#include "../../../hierarchy/not_statements/import_chains/import_star_hides.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<ImportStarHides> ImportStarHides::parse(LangParserState& state) {
		auto out = makeBox<ImportStarHides>(state);

		tpc::Identifier id;

		PARSE().all(&id);
		out->names.push_back(id);

		PST_WHILE(state[0].is(NamedOperator::Period)) {
			PARSE().all(NamedOperator::Period, &id);
			out->names.push_back(id);
		}

		PARSE().one(NamedOperator::PeriodStar);

		if (PARSE().tryEat(Keyword::Hides)) {
			out->hides.emplace();

			PARSE().all(&id);
			out->hides->push_back(id);

			PST_WHILE(state[0].is(Special::Comma)) {
				PARSE().all(Special::Comma, &id);
				out->hides->push_back(id);
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
		addToHash(partial_hash, names.size());
		addToHash(partial_hash, hides.has_value());
		if (hides) addToHash(partial_hash, hides->size());
		return partial_hash;
	}
}
