#pragma once

#include "frontend/pst_parser/access.hpp"
#include "frontend/pst_parser/elements/hierarchy/expr_holders.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp"
#include "preamble.hpp"

#include "base/collections/optional.hpp"

#include "string_id/string_id.hpp"

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
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const final {
			return name.map([](const auto& acc) { return acc.give(); });
		}

		[[nodiscard]] base::StrID getIteratorName() const { return iterator.value; }

		[[nodiscard]] AccessLocked<ExprHolder> getIteratorType() const {
			return type.internal()->getExpr();
		}

		[[nodiscard]] AccessLocked<ExprHolder> getIterable() const {
			return iterable.internal()->getExpr();
		}

		[[nodiscard]] AccessLocked<CodeBlockOrStmt> getBody() const { return body.give(); }

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
