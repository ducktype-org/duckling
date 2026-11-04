// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

#include <lang_definitions/key_spec_op.hpp>

namespace pst::expr {
	/**
	 * @brief Element representing a number value in an expression
	 */
	class ExprNumericValue final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ExprNumericValue, ExprElement, value);

	protected:
		tpc::NumericValue value;

	public:
		[[nodiscard]]
		tpc::NumericValue getValue() const {
			return value;
		}

		/**
		 * @brief The type the literal is written with, like `u8` in `5u8`. `NotATypeSpecifier` for
		 * a literal written without one, like `5`.
		 */
		[[nodiscard]] lang_def::NumericLiteralTypeSpecifier getTypeSpecifier() const;

		explicit ExprNumericValue(LangElementConstructionData state): ExprElement(state, 0) {}

		static MBox<ExprElement> parse(LangParserState& state);

		~ExprNumericValue() override = default;
		void     dprint(std::ostream& out) const final;
		void     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Numeric Value Expr";
		}
	};
}
