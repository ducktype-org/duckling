// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Attribute argument list.
	 */
	class AtrArgList final:
		  public List<UniversalExprHolder, internal::NameGetters::attributeArgList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(AtrArgList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit AtrArgList(LangElementConstructionArgument state): List(state) {
			this->element_kind = ElementKind::AtrArgList;
		}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};
}
