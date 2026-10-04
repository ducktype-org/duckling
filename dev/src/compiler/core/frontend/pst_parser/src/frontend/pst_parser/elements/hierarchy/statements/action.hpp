// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @note Action assumes optional expression before the semicolon.
	 */
	class Action: public Stmt {
		PARENT_CLASS(Stmt);
		THIS_CLASS(Action);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD_OPT(expr, CommaExprHolder);

	public:
		ELEMENT_CLONE_DECL(Action);

		STMT_CHILD_CONSTRUCTOR(Action, ElementKind::Action);
		static MBox<Action> parse(LangParserState& state);
		~Action() override = default;
		[[nodiscard]]
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Action";
		}

		/**
		 * @note Optional of MRef here is intentional
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getValue() const;
	};
}
