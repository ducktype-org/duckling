#pragma once

#include "../lists/parameter_list.hpp"
#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class method element.
	 */
	class Method final: public Stmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(Method, Stmt, is_const);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD_OPT(ret, CommaExprHolder);
		NAMED_CHILD(body, CodeBlockOrStmt);
		bool is_const = false;

		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		STMT_CHILD_CONSTRUCTOR(Method, ElementKind::ClassMethod);
		PARSE_DECL();

		~Method() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Method";
		}

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		AccessLocked<ParamList> getParams() const {
			return params.give();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getRet() const;

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		[[nodiscard]]
		bool isConst() const {
			return is_const;
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
			return false;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
