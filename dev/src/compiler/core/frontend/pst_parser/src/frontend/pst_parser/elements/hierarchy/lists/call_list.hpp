// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../not_statements/call_argument.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Call argument list.
	 */
	class CallList final: public List<CallArgument, internal::NameGetters::callList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CallList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit CallList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::CallList;
		}

		static MBox<CallList> parse(LangParserState& state);

		~CallList() final = default;
	};
}
