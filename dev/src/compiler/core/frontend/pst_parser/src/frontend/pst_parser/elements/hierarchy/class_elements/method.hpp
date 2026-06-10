#pragma once

#include "../lists/parameter_list.hpp"
#include "../meta.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class method element.
	 */
	class Method final: public ClassStmt {
		THIS_CLASS(Method);
		PARENT_CLASS(ClassStmt);
		CLONE_SIGNATURE_DEFAULT_OVERRIDE();
		CLONE_SUBELEMENTS();
	protected:
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(params, ParamList);
		NAMED_CHILD_OPT(ret, CommaExprHolder);
		NAMED_CHILD(body, CodeBlockOrStmt);

		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		ELEMENT_CLONE_DECL(Method);
		CLASS_STMT_CHILD_CONSTRUCTOR(Method, ElementKind::ClassMethod);
		CLASS_STMT_PARSE(Method);

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
