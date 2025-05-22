#pragma once

#include <helios/scope_symbol_id.hpp>

#include <vector>

namespace compiler::helios {
	/**
	 * A simple list of symbols, with additional semantic meaning, that it
	 * is in some way treated as a sym0.sym1.sym2...symN expression.
	 * HELIOS uses it in some places, especially in the lookup and
	 * dealiasing process.
	 */
	struct SymbolList {
		/**
		 * The list of symbols.
		 */
		std::vector<SymID> list;

		// some forwarded vector interface for convenience:

		[[nodiscard]]
		auto back() const {
			return list.back();
		}

		[[nodiscard]]
		auto begin() const {
			return list.begin();
		}

		[[nodiscard]]
		auto end() const {
			return list.end();
		}

		[[nodiscard]]
		bool empty() const {
			return list.empty();
		}
	};
}
