// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../expr_holders.hpp"
#include "../lists/implements_list.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class declaration
	 */
	class Class final: public Decl {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Class, Decl);
		CLONE_SUBELEMENTS();

	private:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(base, ExtendsExprHolder);
		NAMED_CHILD(implements, ImplementsList);
		NAMED_CHILD(body, CodeBlock);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(Class, ElementKind::Class);

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		AccessLocked<CodeBlock> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		AccessLocked<ExtendsExprHolder> getBase() const {
			return base.give();
		}

		[[nodiscard]]
		AccessLocked<ImplementsList> getImplements() const {
			return implements.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return getName();
		}

		static MBox<Class> parse(LangParserState& state);
		~Class() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class";
		}

		// [[nodiscard]]
		// bool isStatementAggregate() const override {
		// 	return true;
		// }

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
