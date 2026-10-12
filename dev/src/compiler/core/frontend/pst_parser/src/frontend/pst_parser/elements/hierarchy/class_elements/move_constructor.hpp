// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../lists/parameter_list.hpp"
#include "class_special.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class move constructor element.
	 */
	class MoveConstructor final: public ClassSpecial {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(MoveConstructor, ClassSpecial);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD(body, CodeBlockOrStmt);

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(MoveConstructor);
		PARSE_DECL();

		~MoveConstructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Move Constructor";
		}

		[[nodiscard]]
		base::Optional<base::StrID> getInternalSymbolName() const final {
			return base::StrID("move");
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
