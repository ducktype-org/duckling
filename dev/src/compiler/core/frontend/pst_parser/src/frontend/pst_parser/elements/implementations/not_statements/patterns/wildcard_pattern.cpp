// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/patterns/wildcard_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	MBox<WildcardPattern> WildcardPattern::parse(LangParserState& state) {
		if (!state[0].is(Special::Underscore)) return nullptr;

		auto out = makeBox<WildcardPattern>(state);
		PARSE().eatOne();
		PST_RETURN out;
	}

	void WildcardPattern::dprint(std::ostream& out) const {
		out << R"({ "pattern_type": "wildcard" })";
	}

	HashAlg& WildcardPattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void WildcardPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitWildcardPattern(*this);
	}
}
