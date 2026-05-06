#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Block declaration
	 */
	class Block final: public CodeDecl {
		NAMED_CHILD_OPT(name, IdentifierWrapper);
		NAMED_CHILD(code_block, CodeBlock);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		explicit Block(const LangParserState& state): CodeDecl(state) {
			element_kind = ElementKind::Block;
		}

		static MBox<Block> parse(LangParserState& state);
		~Block() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Block";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return (name.has_value() ? DeclKind::Symbol : DeclKind::None);
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return name.map([](const auto& x) {return x.give();});
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
