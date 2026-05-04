#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief For declaration
	 */
	class For final: public CodeDecl {
		NAMED_CHILD_OPT(name, IdentifierWrapper);
		NAMED_CHILD(iterator, IdentifierWrapper);
		NAMED_CHILD(type, ForTypeExprHolder);
		NAMED_CHILD(iterable, CommaExprHolder);
		NAMED_CHILD(body, CodeBlockOrStmt);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		explicit For(const LangParserState& state): CodeDecl(state) {
			element_kind = ElementKind::For;
		}

		static MBox<For> parse(LangParserState& state);
		void             dprint(std::ostream& out) const final;
		~For() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "For";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return (name.has_value() ? DeclKind::Symbol : DeclKind::None);
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbol2() const final {
			return name.map([](const auto& acc){ return acc.give(); });
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
