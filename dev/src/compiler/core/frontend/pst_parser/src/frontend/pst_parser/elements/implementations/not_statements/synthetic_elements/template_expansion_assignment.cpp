// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "../../../hierarchy/not_statements/synthetic_elements/template_expansion_assignment.hpp"

#include "../../../synthesizer.hpp"
#include "../preamble.hpp"

namespace pst {
	CLONE_SUB_ELEMENTS_DEF(TemplateExpansionAssignment, name, type, value);

	Box<TemplateExpansionAssignment> ElementSynthesizer::templateExpansionAssignment(
		LangParserElementConstructionData  data,
		MBox<IdentifierWrapper>&&          name,
		base::Optional<MBox<ExprHolder>>&& type,
		MBox<ExprHolder>&&                 value
	) {
		auto out = makeBox<TemplateExpansionAssignment>(data);
		FreeAutomatic::assign(out.refMut(), &out->name, std::move(name));
		if (type) FreeAutomatic::assign(out.refMut(), &out->type, std::move(type).value());
		FreeAutomatic::assign(out.refMut(), &out->value, std::move(value));

		return out;
	}

	HashAlg& TemplateExpansionAssignment::addElementDataToStableHash(HashAlg& partial_hash) const {
		return partial_hash;
	}

	void TemplateExpansionAssignment::dprint(std::ostream& out) const {
		out << "{";
		out << "\"name\" : ";
		nullAwareDprint(name, out);
		if (type.has_value()) {
			out << ", \"type\": ";
			nullAwareDprint(type.value(), out);
		}
		out << ", \"value\": ";
		nullAwareDprint(value, out);
		out << "}";
	}
}
