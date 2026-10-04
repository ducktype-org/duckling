// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../not_statements/patterns/flow_pattern.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief List of flow patterns, used in tuple and deconstructor patterns.
	 */
	class FlowPatternList final: public List<FlowPattern, internal::NameGetters::flowPatternList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(FlowPatternList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit FlowPatternList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::FlowPatternList;
		}

		static MBox<FlowPatternList> parse(LangParserState& state);

		~FlowPatternList() final = default;
	};
}
