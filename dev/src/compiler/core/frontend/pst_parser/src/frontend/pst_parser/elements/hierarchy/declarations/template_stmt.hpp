// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../not_statements/template_decl.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Template declaration
	 */
	class TemplateStmt final: public Decl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateStmt, Decl);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(template_decl, TemplateDecl);
		NAMED_CHILD(inner_statement, Stmt);

		base::Optional<AccessLocked<IdentifierWrapper>> inner_decl_symbol;

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(TemplateStmt, ElementKind::TemplateStmt);

		[[nodiscard]]
		AccessLocked<TemplateDecl> getTemplateDecl() const {
			return template_decl.give();
		}

		[[nodiscard]]
		AccessLocked<Stmt> getInnerStatement() const {
			return inner_statement.give();
		}

		bool trailingSemicolon() override { return false; }

		static MBox<TemplateStmt> parse(LangParserState& state);
		void                      dprint(std::ostream& out) const final;
		~TemplateStmt() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Template Statement";
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return inner_statement.internal()->getDeclSymbolIdentifier();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
