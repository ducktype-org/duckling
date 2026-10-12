// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "format_sub_element.hpp"

namespace pst {
	/**
	 * @brief Element representing an expression group inside of a format string
	 */
	class FormatSubExpression final: public FormatSubElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(FormatSubExpression, FormatSubElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(expr, UniversalAllowBlockExprHolder);

	public:
		[[nodiscard]]
		AccessLocked<UniversalAllowBlockExprHolder> getExpr() const {
			return expr.give();
		}

		explicit FormatSubExpression(LangParserState& state): FormatSubElement(state) {
			this->element_kind = ElementKind::FormatSubExpression;
		}

		static MBox<FormatSubExpression> parse(LangParserState& state);
		~FormatSubExpression() final = default;

		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Format Sub Expression";
		}
	};
}
