#pragma once

#include "../statements/declaration.hpp"

#include "../not_statements/wrapper_elements/identifier_wrapper.hpp" // IWYU pragma: export

#define DECL_CHILD_CONSTRUCTOR(class_name, element_type_)                         \
	class_name(const LangParserState& state): Decl(StmtKind::class_name, state) { \
		this->element_kind = element_type_;                                       \
	}

#define DECL_CHILD_CONSTRUCTOR_NO_KIND(class_name) \
	class_name(const LangParserState& state): Decl(StmtKind::class_name, state) {}

namespace pst {
	/**
	 * @brief Common ancestor element for code declarations.
	 *
	 * Code declarations are statements that can generally create new symbols like function
	 * declarations, variable declarations or language construct with names like fors, blocks and
	 * whiles
	 */
	class CodeDecl: public Decl {
	public:
		DECL_CHILD_CONSTRUCTOR_NO_KIND(CodeDecl);

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Declaration";
		}
	};
}
