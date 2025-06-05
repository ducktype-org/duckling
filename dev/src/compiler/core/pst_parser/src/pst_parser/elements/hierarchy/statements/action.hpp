#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @note Action assumes optional expression before the semicolon.
	 */
	class Action: public Stmt {
	protected:
		base::Optional<AccessInternal<CommaExprHolder>> expr;

	public:
		STMT_CHILD_CONSTRUCTOR(Action, ElementKind::Action);
		static MBox<Action> parse(LangParserState& state);
		~Action() override = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Action";
		}

		[[nodiscard]]
		/**
		 * @note Optional of MRef here is intentional
		 */
		base::Optional<AccessLocked<ExprHolder>> getValue() const {
			return expr.map([](const auto& e) -> AccessLocked<ExprHolder> { return e.give(); });
		}
	};
}
