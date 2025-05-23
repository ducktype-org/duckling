#pragma once

#include "../not_statements.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Block declaration
	 */
	class Block final: public CodeDecl {
		tpc::OptionalIdentifier   optional_name;
		AccessInternal<CodeBlock> code_block;

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

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
