// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Expression surrounded by parenthesis.
	 */
	class RoundGroupExpr final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(RoundGroupExpr, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(expr, CommaAllowBlocksExprHolder);

	public:
		explicit RoundGroupExpr(const LangParserState& state): NotStmt(state) {
			this->element_kind = ElementKind::RoundGroupExpr;
		}

		static MBox<RoundGroupExpr> parse(LangParserState& state);
		~RoundGroupExpr() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Round Group Expression";
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getExpr() const {
			return expr.give();
		}
	};
}
