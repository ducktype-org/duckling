#pragma once

#include "../meta.hpp"
#include "../not_statements/wrapper_elements/identifier_wrapper.hpp"
#include "../not_statements/wrapper_elements/keyword_wrapper.hpp"
#include "preamble.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace pst {
	/**
	 * @brief Special class methods like constructors, and destructors using the
	 * `ClassName.type(...)` syntax
	 */
	class ClassSpecial: public Stmt {
		PARENT_CLASS(Stmt);
		THIS_CLASS(ClassSpecial);

	protected:
		ELEMENT_CLONE_DECL(ClassSpecial);
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		ClassSpecial(StmtKind kind, const LangParserState& state): Stmt(kind, state) {}

		PARSE_DECL();

		[[nodiscard]]
		std::string elementType() const override {
			return "Class special method";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]]
		bool trailingSemicolon() final {
			return false;
		}
	};
}
