// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/class_elements/field.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Field, name, type, init);

	MBox<Field> Field::parse(LangParserState& state) {
		auto out = makeBox<Field>(state);

		// We declare fields without `let` or `var` so `let x: T` or `var x: T` are reported as it
		// may be a common mistake.
		if (state[0].is(Keyword::Let) || state[0].is(Keyword::Var)) {
			state.logSafeError(
				makeBox<FieldBindingKeywordError>(
					state.getPosition(), lang_def::keywordToStr(state[0].asKeyword()).str()
				)
			);
			state.tokens().skip();
		}

		PARSE().all(&out->name, NamedOperator::Colon);
		PARSE().one(&out->type);

		if (PARSE().tryEat(NamedOperator::Assign)) PARSE().one(&out->init);

		PST_RETURN out;
	}

	void Field::dprint(std::ostream& out) const {
		out << "{";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(, "type": )";
		nullAwareDprint(type, out);

		if (init) {
			out << R"(, "initial": )";
			nullAwareDprint(init.value(), out);
		}

		out << "}";
	}

	HashAlg& Field::addElementDataToStableHash(HashAlg& partial_hash) const { return partial_hash; }

	void Field::acceptVisitor(PstVisitor& visitor) const { visitor.visitField(*this); }
}
