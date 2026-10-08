// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/synthetic_elements/template_expansion_assignment.hpp"
#include "../../../synthesizer.hpp"
#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(TemplateTopLevel, assignments, stmt);

	Box<TemplateTopLevel> ElementSynthesizer::templateTopLevel(
		LangParserElementConstructionData                data,
		std::vector<MBox<TemplateExpansionAssignment>>&& assignments,
		MBox<Stmt>&&                                     stmt
	) {
		auto out = makeBox<TemplateTopLevel>(data);

		for (auto& box: std::move(assignments)) {
			out->assignments.emplace_back(nullptr);
			FreeAutomatic::assign(out.refMut(), &out->assignments.back(), std::move(box));
		}
		FreeAutomatic::assign(out.refMut(), &out->stmt, std::move(stmt));

		return out;
	}

	HashAlg& TemplateTopLevel::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void TemplateTopLevel::dprint(std::ostream& out) const {
		out << "{";
		out << "\"assignments\" : [";
		for (auto& assi: assignments) {
			nullAwareDprint(assi, out);
			out << ",";
		}
		out << "], \"stmt\": ";
		nullAwareDprint(stmt, out);
		out << "}";
	}
}
