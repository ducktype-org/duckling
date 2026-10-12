// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/not_statements/selector.hpp"

#include "../../hierarchy/lists/selector_list.hpp"  // IWYU pragma: keep
#include "preamble.hpp"

namespace pst {
	namespace {
		std::string_view tailName(SelectorTail tail) {
			switch (tail) {
			case SelectorTail::None:
				return "none";
			case SelectorTail::Star:
				return "star";
			case SelectorTail::Nested:
				return "nested";
			case SelectorTail::As:
				return "as";
			}
			return "unknown";
		}
	}

	CLONE_SUB_ELEMENTS_DEF(Selector, names, as_name, nested, hides);

	MBox<Selector> Selector::parse(LangParserState& state) {
		auto out = makeBox<Selector>(state);

		MBox<IdentifierWrapper> id;

		auto push_id = [&](auto& id_list) {
			id_list.emplace_back(nullptr);
			PARSE().assign(&id_list.back(), std::move(id));
		};

		PARSE().one(&id);
		push_id(out->names);

		PST_WHILE(state[0].is(NamedOperator::Period) && state[1].isIdentifier()) {
			PARSE().all(NamedOperator::Period, &id);
			push_id(out->names);
		}

		if (PARSE().tryEat(NamedOperator::PeriodStar)) {
			out->tail_kind = SelectorTail::Star;

			if (PARSE().tryEat(Keyword::Hides)) {
				if (state[0].isBracketGroup(Token::BracketType::Curly)) {
					PARSE().goDown();
					do {
						PARSE().one(&id);
						push_id(out->hides);
					}
					PST_WHILE(PARSE().tryEat(Special::Comma) && state.notEmpty());
					PARSE().goUpAndSkip();
				} else {
					PARSE().one(&id);
					push_id(out->hides);
				}
			}
		} else if (state[0].is(NamedOperator::Period)
		           && state[1].isBracketGroup(Token::BracketType::Curly)) {
			out->tail_kind = SelectorTail::Nested;

			MBox<NestedSelectorList> nested_list;
			PARSE().all(NamedOperator::Period, &nested_list);
			PARSE().assign(&out->nested, std::move(nested_list));
		} else if (PARSE().tryEat(NamedOperator::As)) {
			out->tail_kind = SelectorTail::As;

			PARSE().one(&id);
			PARSE().assign(&out->as_name, std::move(id));
		}

		// Anything left is not a valid tail, e.g. `a.b.`, `a.b(c)` or `a.* as d`.
		if (state.notEmpty()) state.logInt(makeBox<BadSelectorError>(state[0].getPosition()));

		PST_RETURN out;
	}

	base::Optional<AccessLocked<IdentifierWrapper>> Selector::getDeclaredName() const {
		switch (tail_kind) {
		case SelectorTail::As:
			return getAsName();
		case SelectorTail::None:
			if (names.empty()) return {};
			return names.back().give();
		case SelectorTail::Star:
		case SelectorTail::Nested:
			return {};
		}
		return {};
	}

	void Selector::dprint(std::ostream& out) const {
		out << "{";

		out << R"("names": [)";
		for (const auto& name: names) {
			nullAwareDprint(name, out);
			out << ",";
		}
		out << "],";

		out << R"("tail": ")" << tailName(tail_kind) << R"(")";

		if (as_name) {
			out << R"(,"as": )";
			nullAwareDprint(*as_name, out);
		}

		if (nested) {
			out << R"(,"nested": )";
			nullAwareDprint(*nested, out);
		}

		if (!hides.empty()) {
			out << R"(,"hides": [)";
			for (const auto& name: hides) {
				nullAwareDprint(name, out);
				out << ",";
			}
			out << "]";
		}

		out << "}";
	}

	HashAlg& Selector::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, names.size());
		addToHash(partial_hash, static_cast<int>(tail_kind));
		addToHash(partial_hash, hides.size());
		return partial_hash;
	}

	void Selector::calcElementPathHashRecursive() {
		calcIndexedListChildPath<IdentifierWrapper>({ names }, { getElementPathHash(), "names" });
		calcIndexedListChildPath<IdentifierWrapper>({ hides }, { getElementPathHash(), "hides" });
		if (as_name.has_value()) calcNamedChildPath(as_name.value(), getElementPathHash());
		if (nested.has_value()) calcNamedChildPath(nested.value(), getElementPathHash());
	}
}
