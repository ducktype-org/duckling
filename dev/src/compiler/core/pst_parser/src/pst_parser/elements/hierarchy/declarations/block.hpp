#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Block declaration
	 */
	class Block final: public CodeDecl {
		tpc::OptionalIdentifier   optional_name;
		NAMED_CHILD(code_block, CodeBlock);

	public:
		explicit Block(const dia::SourcePosition& position): CodeDecl(position) {
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
		bool isDeclaration() const final {
			return optional_name.value.has_value();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
