#pragma once

#include "frontend/pst_parser/access.hpp"
#include "frontend/pst_parser/elements/hierarchy/expr_holders.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/wrapper_elements/identifier_wrapper.hpp"
#include "preamble.hpp"

#include "base/collections/optional.hpp"

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
		base::Optional<bool> is_const;

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

		[[nodiscard]] AccessLocked<IdentifierWrapper> getIteratorIdentifier() const {
			return iterator.give();
		}

		[[nodiscard]] AccessLocked<ForTypeExprHolder> getIteratorType() const {
			return type.give();
		}

		[[nodiscard]] AccessLocked<CommaExprHolder> getIterable() const { return iterable.give(); }

		[[nodiscard]] AccessLocked<CodeBlockOrStmt> getBody() const { return body.give(); }

		[[nodiscard]] base::Optional<bool> getIsConst() const { return is_const; }

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
