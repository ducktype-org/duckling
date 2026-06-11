#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class field element.
	 */
	class Field final: public ClassStmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Field, ClassStmt, is_mutable);
		CLONE_SUBELEMENTS();
	protected:
		bool is_mutable = true;
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(type, CommaExprHolder);
		NAMED_CHILD_OPT(init, CommaExprHolder);

		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Field, ElementKind::ClassField);
		CLASS_STMT_PARSE(Field);

		~Field() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Field";
		}

		[[nodiscard]]
		bool isMutable() const {
			return is_mutable;
		}

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getType() const {
			return type.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getInit() const {
			if (init.has_value()) return { init.value().give() };
			return {};
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return getName();
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
