// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/patterns/deconstructor_pattern.hpp"

#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(DeconstructorPattern, deconstructor_name, arguments);

	MBox<DeconstructorPattern> DeconstructorPattern::parse(LangParserState& state) {
		auto out = makeBox<DeconstructorPattern>(state);

		PARSE().all(&out->deconstructor_name, &out->arguments);
		PST_RETURN out;
	}

	void DeconstructorPattern::dprint(std::ostream& out) const {
		out << "{";
		out << R"("pattern_type": "deconstructor",)";
		out << R"("name": )";
		nullAwareDprint(deconstructor_name, out);
		out << R"(, "arguments": [)";
		nullAwareDprint(arguments, out);
		out << "]}";
	}

	HashAlg& DeconstructorPattern::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void DeconstructorPattern::acceptVisitor(PstVisitor& visitor) const {
		visitor.visitDeconstructorPattern(*this);
	}
}
