#pragma once

#include "../../lang_state_unmethods.hpp"
#include "../meta.hpp"                         // IWYU pragma: export
#include "../not_statements/expr_element.hpp"  // IWYU pragma: export
#include "../not_statements/wrapper_elements/identifier_wrapper.hpp" // IWYU pragma: export

#define CONDITION(name) static bool name(const TokenStream& state, i64 fwd = 0)

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
}
