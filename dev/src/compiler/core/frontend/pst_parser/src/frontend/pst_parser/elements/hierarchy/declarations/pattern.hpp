#pragma once

#include "../not_statements/param.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Pattern declaration.
	 */
	class Pattern final: public Decl {
		NAMED_CHILD(name, IdentifierWrapper);
		NAMED_CHILD(param, Param);
		NAMED_CHILD_OPT(ret, CommaExprHolder);
		NAMED_CHILD(body, CodeBlockOrStmt);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(Pattern, ElementKind::Pattern);

		[[nodiscard]]
		AccessLocked<IdentifierWrapper> getName() const {
			return name.give();
		}

		[[nodiscard]]
		AccessLocked<Param> getParam() const {
			return param.give();
		}

		/**
		 * @note Optional of MCRef here is intentional
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getRet() const {
			return ret.map([](const auto& v) -> AccessLocked<ExprHolder> { return v.give(); });
		}

		[[nodiscard]]
		AccessLocked<CodeBlockOrStmt> getBody() const {
			return body.give();
		}

		static MBox<Pattern> parse(LangParserState& state);
		void                 dprint(std::ostream& out) const final;
		~Pattern() final = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Pattern";
		}

		[[nodiscard]]
		base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbol2() const override {
			return getName();
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
