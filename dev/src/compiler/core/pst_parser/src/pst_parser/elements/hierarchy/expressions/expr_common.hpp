#pragma once

#include "../../lang_state_unmethods.hpp"
#include "../meta.hpp"                         // IWYU pragma: export
#include "../not_statements/expr_element.hpp"  // IWYU pragma: export

#define CONDITION(name) static bool name(const LangParserState& state, i64 fwd = 0)

namespace pst {
	/**
	 * @brief For now these are some more general expr classification functions.
	 *
	 * @note This is temporary, it will be improved in the future.
	 */
	class ExprClassify {
	public:
		ExprClassify() = delete;

		CONDITION(isComparison);
		CONDITION(isAssignment);
		CONDITION(exprStmtEnd);
	};

	namespace expr {
		/**
		 * @brief General parseUntil that allows to parse an expression element with a condition for
		 * expression end.
		 */
		template<std::derived_from<ExprElement> T, StateCondition until>
		MBox<ExprElement> parseUntil(LangParserState& state) {
			i64 length = 0;
			while (!detail::isSentinel(state, length) && !until(state, length)) length++;
			return T::parse(state, length);
		}
	}
}
