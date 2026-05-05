#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Block declaration
	 */
	class Block final: public CodeDecl {
		tpc::OptionalIdentifier optional_name;
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

		[[nodiscard]] pst::AccessLocked<CodeBlock> getCodeBlock() const {
			return code_block.give();
		}

		[[nodiscard]] base::Optional<base::StrID> getName() const {
			return optional_name.value;
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return (optional_name.value ? DeclKind::Symbol : DeclKind::None);
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return optional_name.value;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
