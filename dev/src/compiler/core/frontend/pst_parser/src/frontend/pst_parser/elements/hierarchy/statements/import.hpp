#pragma once

#include "../meta.hpp"

namespace pst {
	class Import final: public Stmt {
		NAMED_CHILD(import_chain, ImportChain);

	public:
		STMT_CHILD_CONSTRUCTOR(Import, ElementKind::Import);
		static MBox<Import> parse(LangParserState& state);
		[[nodiscard]]

		[[nodiscard]]
		bool getStar() const;
		~Import() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Import";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return {};
		}
	};
}
