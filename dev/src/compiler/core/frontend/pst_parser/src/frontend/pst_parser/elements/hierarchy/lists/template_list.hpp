// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Template initialization list.
	 */
	class TemplateList final:
		  public List<UniversalExprHolderLowerLevel, internal::NameGetters::templateList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit TemplateList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::TemplateList;
		}

		static MBox<TemplateList> parse(LangParserState& state);

		~TemplateList() final = default;
	};
}
