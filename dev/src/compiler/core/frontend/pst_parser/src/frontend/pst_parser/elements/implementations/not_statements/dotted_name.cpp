#include "../../hierarchy/not_statements/dotted_name.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(DottedName, names)

	bool DottedName::getStar() const { return star; }

	MBox<DottedName> DottedName::parse(LangParserState& state) {
		auto out = makeBox<DottedName>(state);
		do {
			MBox<IdentifierWrapper> id;
			PARSE().one(&id);
			out->names.emplace_back();
			PARSE().assign(&out->names.back(), std::move(id));
		}
		PST_WHILE(PARSE().tryEat(lang_def::NamedOperator::Period));

		if (PARSE().tryEat(lang_def::NamedOperator::PeriodStar)) out->star = true;

		PST_RETURN out;
	}

	void DottedName::dprint(std::ostream& out) const {
		out << "{";

		if (star)
			out << R"("star": "true",)";
		else
			out << R"("star": "false",)";

		out << R"("names": [)";

		for (const auto& name: names) {
			nullAwareDprint(name, out);
			out << ", ";
		}

		out << "]}";
	}

	HashAlg& DottedName::addElementDataToStableHash(HashAlg& partial_hash) const {
		hashing::addToHash(partial_hash, names.size());
		hashing::addToHash(partial_hash, star);
		return partial_hash;
	}

	void DottedName::calcElementPathHashRecursive() {
		calcIndexedListChildPath<IdentifierWrapper>({ names }, getElementPathHash());
	}
}
