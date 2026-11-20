// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Top-level element that is the root of the pst of a single file.
	 */
	class TopLevel final: public Decl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TopLevel, Decl, type);
		CLONE_SUBELEMENTS();

	protected:
		std::vector<AccessInternalAnonymous<Stmt>> statements;
		/**
		 * @brief The ordering in the top-level is based on context and usually changed by passing
		 * the correct PSTType option to PST or a whole custom context.
		 */
		BlockOrderType type = BlockOrderType::Undefined;

		/**
		 * This is the division of statements inside the block based on their symbol declaration
		 * kind and the symbol they declare, more detailed information about symbol declaration
		 * kinds is in the declaration of Stmt.
		 */
		base::Map<base::StrID, std::vector<AccessLocked<Stmt>>> by_symbol;
		std::vector<AccessLocked<Stmt>>                         no_symbol;
		std::vector<AccessLocked<Stmt>>                         transparent;

		/**
		 * @brief Fills the by_symbol, no_symbol and transparent variables to reflect an ordered
		 * code block.
		 */
		void fillSymbols();

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(TopLevel, ElementKind::TopLevel);

		static MBox<TopLevel> parse(LangParserState& state);

		~TopLevel() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Top Level";
		}

		[[nodiscard]]
		auto getStatements() const {
			using namespace std::views;
			static auto give_one = [](auto& acc) -> AccessLocked<Stmt> { return acc.give(); };
			return std::ranges::ref_view(statements) | transform(give_one);
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}

		void calcElementPathHashRecursive() override;
	};
}
