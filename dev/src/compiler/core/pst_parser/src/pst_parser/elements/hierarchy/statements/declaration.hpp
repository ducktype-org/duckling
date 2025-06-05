#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @todo should assert be an action
	 */
	class Decl: public Stmt {
	public:
		Decl(StmtKind kind, const dia::SourcePosition& position): Stmt(kind, position) {}

		bool trailingSemicolon() override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Declaration";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}
	};
}
