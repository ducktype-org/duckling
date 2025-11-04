#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @note Action assumes optional expression before the semicolon.
	 */
	class Action: public Stmt {
	protected:
		NAMED_CHILD_OPT(expr, CommaExprHolder);

	public:
		STMT_CHILD_CONSTRUCTOR(Action, ElementKind::Action);
		static MBox<Action> parse(LangParserState& state);
		~Action() override = default;
		[[nodiscard]]
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Action";
		}

		/**
		 * @note Optional of MRef here is intentional
		 */
		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getValue() const;
	};
}
