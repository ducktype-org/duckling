// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../not_statements/param.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Function declaration parameter list.
	 */
	class ParamList final: public List<Param, internal::NameGetters::parameterList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ParamList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit ParamList(LangElementConstructionArgument state): List(state) {
			this->element_kind = ElementKind::ParamList;
		}

		static MBox<ParamList> parse(LangParserState& state);

		~ParamList() final = default;
	};
}
