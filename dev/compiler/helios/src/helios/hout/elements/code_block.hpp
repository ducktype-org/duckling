#pragma once

#include "../../scope_symbol_id.hpp"

#include <vector>

#include <base/box.hpp>

namespace compiler::helios::code {
	struct Stmt;

	/**
	 * @brief A block of HOUT statements
	 */
	struct CodeBlock final {
		ScopeID                      lifetime_scope;
		std::vector<base::Box<Stmt>> statements;
	};
}
