// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../hierarchy/not_statements/param.hpp"

#include "preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(Param, name, type, initial);

	MBox<Param> Param::parse(LangParserState& state) {
		auto out = makeBox<Param>(state);

		PARSE().all(&out->name, NamedOperator::Colon);

		PARSE().one(&out->type);

		if (PARSE().tryEat(NamedOperator::Assign)) PARSE().one(&out->initial);

		PST_RETURN out;
	}

	base::Optional<AccessLocked<UniversalExprHolder>> Param::getValue() const {
		return initial.map([](const auto& v) { return v.give(); });
	}

	void Param::dprint(std::ostream& out) const {
		out << "{\"Parameter\": {";

		out << R"("name": )";
		nullAwareDprint(name, out);
		out << R"(,"type": )";
		nullAwareDprint(type, out);
		if (initial) {
			out << R"(,"initial": )";
			nullAwareDprint(initial.value(), out);
		}

		out << "}}";
	}

	HashAlg& Param::addElementDataToStableHash(HashAlg& partial_hash) const {
		addToHash(partial_hash, initial.has_value());
		return partial_hash;
	}

	void Param::acceptVisitor(PstVisitor& visitor) const { visitor.visitParam(*this); }
}
