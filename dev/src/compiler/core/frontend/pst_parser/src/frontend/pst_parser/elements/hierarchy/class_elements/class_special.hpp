#pragma once

#include "../meta.hpp"
#include "preamble.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace pst {
	/**
	 * @brief Special class methods like constructors, and destructors using the
	 * `ClassName.type(...)` syntax
	 */
	class ClassSpecial: public ClassStmt {
	protected:
		/* What is after the `.`, It may be a keyword in some cases(for now it's only the move constructor) */
		NAMED_CHILD_OPT(ident, IdentifierWrapper);
		NAMED_CHILD_OPT(key, KeywordWrapper);

		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		CLASS_STMT_PASS_CONSTRUCTOR(ClassSpecial);
		CLASS_STMT_PARSE(ClassSpecial);

		[[nodiscard]]
		std::string elementType() const override {
			return "Class special method";
		}

		[[nodiscard]]
		base::StrID getName() const {
			using namespace tpc;
			base::StrID res;
			VARIANT_VISIT(
				kind,
				VISIT_CASE(Identifier, ident, res = base::StrID(ident)),
				VISIT_CASE(Keyword, key, res = keywordToStr(key))
			);
			return res;
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
		bool trailingSemicolon() final {
			return false;
		}
	};
}
