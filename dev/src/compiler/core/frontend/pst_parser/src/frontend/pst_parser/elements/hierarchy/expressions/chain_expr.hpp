// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "expr_common.hpp"

namespace pst::expr {
	/**
	 * @brief Combined chain of an atom followed by Accesses / Calls / Subscripts.
	 */
	class ChainExpr final: public ExprElement {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ChainExpr, ExprElement);
		CLONE_SUBELEMENTS();

	protected:
		using Lower = Atom;

		NAMED_CHILD(atom, ExprElement);
		std::vector<AccessInternalAnonymous<ExprElement>> chain;

		/**
		 * @brief checks length before the start of the next link
		 */
		static i64 toNextLink(const LangParserState& state);

	public:
		ChainExpr(const LangParserState& state): ExprElement(state, 300) {}

		static MBox<ExprElement> parse(LangParserState& state);
		void                     dprint(std::ostream& out) const final;
		void                     acceptExprVisitor(PstExprVisitor& visitor) const final;
		HashAlg&                 addElementDataToStableHash(HashAlg&) const override;

		~ChainExpr() override = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Chain Expression";
		}

		[[nodiscard]]
		AccessLocked<ExprElement> getAtom() const;

		[[nodiscard]]
		auto getChain() const {
			using namespace std::views;
			static auto give_one
				= [](const auto& ref) -> AccessLocked<ExprElement> { return ref.give(); };
			return std::ranges::ref_view(chain) | transform(give_one);
		}

		void calcElementPathHashRecursive() override;
	};
}
