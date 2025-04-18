#pragma once

#include "elements/hierarchy/statements.hpp"
#include "pst.hpp"

namespace pst {
	/**
	 * @brief Helper class for macro expansion implementations. It needs to be an externall class so
	 * that it can be a friend of pst::Expand  while also being able to return a pst.
	 */
	class Expander {
	public:
		Expander()                 = delete;
		Expander(const Expander&)  = delete;
		Expander(const Expander&&) = delete;

		/**
		 * @brief This is a bit of a temporary solution. For now this will create a temporary file
		 * to parse. There will need to be new errors/positions to handle expand macros properly.
		 */
		static PST<Stmt> expandStmt(Access<Expand> exp) {
			return PST<Stmt>::fromContents(exp->string.str());
		}
	};
}
