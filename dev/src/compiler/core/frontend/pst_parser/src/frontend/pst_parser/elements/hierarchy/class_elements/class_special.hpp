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
	class ClassSpecial: public ClassStmt {
	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		CLASS_STMT_PASS_CONSTRUCTOR(ClassSpecial);
		CLASS_STMT_PARSE(ClassSpecial);

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
