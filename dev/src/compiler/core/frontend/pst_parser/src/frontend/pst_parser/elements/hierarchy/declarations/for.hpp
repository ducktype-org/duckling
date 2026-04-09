#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief For declaration
	 */
	class For final: public CodeDecl {
		tpc::OptionalIdentifier optional_name;
		tpc::Identifier         iterator;
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
			return (optional_name.value ? DeclKind::Symbol : DeclKind::None);
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return optional_name.value;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
