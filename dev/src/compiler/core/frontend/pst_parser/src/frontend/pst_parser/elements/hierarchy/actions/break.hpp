// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../statements/action.hpp"

namespace pst {
	class Break final: public Action {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Break, Action);

	public:
		explicit Break(const LangParserState& state): Action(state) {}

		void dprint(std::ostream& out) const final;
		~Break() final = default;

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
