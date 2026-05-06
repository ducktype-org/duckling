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
		/* What is after the `.`, It may be a keyword in some cases(for now it's only the move
		 * constructor) */
		bool implied_constructor = false;
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
		bool isImpliedConstructor() const {
			return implied_constructor;
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getIdentifier() const {
			return ident.map([](const auto& acc) { return acc.give(); });
		}

		[[nodiscard]]
		base::Optional<AccessLocked<KeywordWrapper>> getKeyword() const {
			return key.map([](const auto& acc) { return acc.give(); });
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]]
		base::Optional<base::StrID> getInternalSymbolName() const final {
			if (implied_constructor)
				return base::StrID("create");
			else if (ident)
				return ident->internal()->unwrap();
			else if (key)
				return lang_def::keywordToStr(key->internal()->unwrap());
			CORE_UNREACHABLE();
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return ident.map([](const auto& acc) { return acc.give(); });
		}

		[[nodiscard]]
		bool trailingSemicolon() final {
			return false;
		}
	};
}
