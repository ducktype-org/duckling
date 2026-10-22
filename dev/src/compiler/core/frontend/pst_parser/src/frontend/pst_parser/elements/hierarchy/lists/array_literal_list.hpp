// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Array literal list.
	 */
	class ArrayLiteralList final:
		  public List<UniversalExprHolder, internal::NameGetters::arrayLiteralList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ArrayLiteralList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit ArrayLiteralList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::ArrayLiteralList;
		}

		static MBox<ArrayLiteralList> parse(LangParserState& state);

		~ArrayLiteralList() final = default;
	};
}
