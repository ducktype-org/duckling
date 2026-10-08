// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../expr_holders.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class implements list.
	 */
	class ImplementsList final:
		  public List<ImplementsElementExprHolder, internal::NameGetters::inheritanceList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ImplementsList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit ImplementsList(LangElementConstructionArgument state): List(state) {}

		static MBox<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};
}
