#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Function declaration
	 */
	class SpecifierBlock final: public Stmt {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(SpecifierBlock, Stmt);
		CLONE_SUBELEMENTS();

	protected:
		NAMED_CHILD(block, CodeBlock);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		STMT_CHILD_CONSTRUCTOR(SpecifierBlock, ElementKind::SpecifierBlock);

		[[nodiscard]]
		AccessLocked<CodeBlock> getBlock() const {
			return block.give();
		}

		static MBox<SpecifierBlock> parse(LangParserState& state);
		void                        dprint(std::ostream& out) const final;
		~SpecifierBlock() final = default;
		bool trailingSemicolon() override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Specifier block";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
