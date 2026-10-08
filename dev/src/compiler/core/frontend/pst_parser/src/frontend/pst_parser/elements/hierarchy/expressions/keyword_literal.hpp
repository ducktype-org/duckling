// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../not_statements/wrapper_elements/keyword_wrapper.hpp"  // IWYU pragma: keep
#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Element that represents an keyword literal in an expression
	 */
	class KeywordLiteral final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(KeywordLiteral, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(keyword, KeywordWrapper);

	public:
		explicit KeywordLiteral(LangElementConstructionArgument state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~KeywordLiteral() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		AccessLocked<KeywordWrapper> getKeyword() const {
			return keyword.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Keyword Expression";
		}
	};
}
