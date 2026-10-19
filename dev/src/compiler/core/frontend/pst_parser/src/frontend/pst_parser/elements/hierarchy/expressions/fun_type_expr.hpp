// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Function type expression, for example `fun(i32, i64) -> bool`.
	 *
	 * The parameters are a list of type expressions. The return type is optional, a missing one
	 * means that the function returns unit.
	 */
	class FunTypeExpr final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(FunTypeExpr, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		std::vector<AccessInternalAnonymous<ExprElement>> params;
		NAMED_CHILD_OPT(ret, ExprElement);

	public:
		explicit FunTypeExpr(const LangParserState& state): ExprElement(state, 300) {}

		/**
		 * @brief Parses `fun(<params>)` optionally followed by `-> <return type>`.
		 */
		static MBox<ExprElement> parse(LangParserState& state);
		void                     dprint(std::ostream& out) const final;
		void                     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg&                 addElementDataToStableHash(HashAlg&) const override;

		~FunTypeExpr() override = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function Type Expression";
		}

		[[nodiscard]]
		auto getParams() const {
			using namespace std::views;
			static auto give_one
				= [](const auto& ref) -> AccessLocked<ExprElement> { return ref.give(); };
			return std::ranges::ref_view(params) | transform(give_one);
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprElement>> getRet() const;

		void calcElementPathHashRecursive() override;
	};
}
