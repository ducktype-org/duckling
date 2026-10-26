// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../lists/parameter_list.hpp"
#include "preamble.hpp"

#include <token_parser_core/common_elements.hpp>

namespace pst {
	/**
	 * @brief Function declaration
	 */
	class Fun final: public Decl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Fun, Decl, operator_fixity);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD_OPT(ret, CommaExprHolder);
		NAMED_CHILD(body, CodeBlockOrStmt);
		OperatorFixity operator_fixity = OperatorFixity::None;

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(Fun, ElementKind::Fun);

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		[[nodiscard]]
		OperatorFixity getOperatorFixity() const {
			return operator_fixity;
		}

		/**
		 * @note Optional of MCRef here is intentional
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getRet() const;

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		static MBox<Fun> parse(LangParserState& state);
		void             dprint(std::ostream& out) const final;
		~Fun() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Function";
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return getName();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
