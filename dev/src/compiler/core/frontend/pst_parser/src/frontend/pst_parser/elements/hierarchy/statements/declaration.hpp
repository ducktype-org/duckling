#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @todo should assert be an action
	 */
	class Decl: public Stmt {
		THIS_CLASS(Decl);
		PARENT_CLASS(Stmt);
	public:
		ELEMENT_CLONE_DECL(Decl);
		Decl(StmtKind kind, const LangParserState& state): Stmt(kind, state) {}

		bool trailingSemicolon() override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Declaration";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const override {
			return DeclKind::Symbol;
		}
	};
}
