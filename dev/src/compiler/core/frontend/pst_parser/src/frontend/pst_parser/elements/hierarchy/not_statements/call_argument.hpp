// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../meta.hpp"

#include <base/collections/optional.hpp>

namespace pst {
	/**
	 * @brief Call argument, handles both named and normal arguments.
	 */
	class CallArgument final: public NotStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CallArgument, NotStmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD_OPT(arg_name, IdentifierWrapper);
		NAMED_CHILD(arg, UniversalExprHolderLowerLevel);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit CallArgument(LangElementConstructionArgument state): NotStmt(state) {
			this->element_kind = ElementKind::CallArgument;
		}

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Call argument";
		}

		[[nodiscard]]
		bool isNamedArg() const {
			return arg_name.has_value();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getArgName() const {
			return arg_name.map([](const auto& acc) { return acc.give(); });
		}

		[[nodiscard]]
		AccessLocked<UniversalExprHolderLowerLevel> getArg() const {
			return arg.give();
		}

		static MBox<CallArgument> parse(LangParserState& state);

		~CallArgument() final = default;
		void dprint(std::ostream& out) const final;
	};
}
