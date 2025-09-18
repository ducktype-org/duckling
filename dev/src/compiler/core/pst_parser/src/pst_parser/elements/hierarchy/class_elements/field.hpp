#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class field element.
	 */
	class Field final: public ClassStmt {
		bool            is_mutable = true;
		tpc::Identifier name;
		NAMED_CHILD(type, CommaExprHolder);
		NAMED_CHILD_OPT(init, CommaExprHolder);

		HashAlg& calcStableHash(HashAlg& partial_hash) const override;
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
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getType() const {
			return type.give();
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return getName();
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
