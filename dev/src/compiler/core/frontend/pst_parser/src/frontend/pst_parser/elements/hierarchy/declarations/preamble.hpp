#pragma once

#include "../not_statements/wrapper_elements/identifier_wrapper.hpp"  // IWYU pragma: export
#include "../statements/declaration.hpp"

#define DECL_CHILD_CONSTRUCTOR(class_name, element_type_)                                  \
	class_name(LangElementConstructionArgument state): Decl(StmtKind::class_name, state) { \
		this->element_kind = element_type_;                                                \
	}

#define DECL_CHILD_CONSTRUCTOR_NO_KIND(class_name) \
	class_name(LangElementConstructionArgument state): Decl(StmtKind::class_name, state) {}

namespace pst {
	/**
	 * @brief Common ancestor element for code declarations.
	 *
	 * Code declarations are statements that can generally create new symbols like function
	 * declarations, variable declarations or language construct with names like fors, blocks and
	 * whiles
	 */
	class CodeDecl: public Decl {
		THIS_CLASS(CodeDecl);
		PARENT_CLASS(Decl);

	public:
		ELEMENT_CLONE_DECL(CodeDecl);
		DECL_CHILD_CONSTRUCTOR_NO_KIND(CodeDecl);

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Declaration";
		}
	};
}
