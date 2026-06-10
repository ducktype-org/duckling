#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @note Action assumes optional expression before the semicolon.
	 */
	class Action: public Stmt {
		PARENT_CLASS(Stmt);
		THIS_CLASS(Action);
	protected:
		CLONE_SUBELEMENTS();

		NAMED_CHILD_OPT(expr, CommaExprHolder);
	public:
		/**
		 * @todo handle expression
		 */
		ELEMENT_CLONE_DECL(Action);

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
