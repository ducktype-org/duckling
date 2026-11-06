// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../meta.hpp"

#include <lang_definitions/key_spec_op.hpp>

namespace pst {
	[[nodiscard]]
	constexpr bool isControlFlowTargetKeyword(lang_def::Keyword keyword) {
		switch (keyword) {
		case lang_def::Keyword::If:
		case lang_def::Keyword::While:
		case lang_def::Keyword::For:
		case lang_def::Keyword::Block:
			return true;
		default:
			return false;
		}
	}

	/**
	 * @note Action may contain an expression or a block-kind target before the semicolon.
	 */
	class Action: public Stmt {
		PARENT_CLASS(Stmt);
		THIS_CLASS(Action);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD_OPT(expr, CommaExprHolder);
		base::Optional<lang_def::Keyword> target_keyword;

	public:
		ELEMENT_CLONE_DECL(Action, target_keyword);

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

		[[nodiscard]]
		base::Optional<lang_def::Keyword> getTargetKeyword() const {
			return target_keyword;
		}
	};
}
