// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/symbols/symbol_id.hpp>

#include <vector>

namespace compiler::helios {
	/**
	 * A simple list of symbols, with additional semantic meaning, that it
	 * is in some way treated as a sym0.sym1.sym2...symN expression.
	 * HELIOS uses it in some places, especially in the lookup and
	 * dealiasing process.
	 */
	struct SymbolList final {
		/**
		 * The list of symbols.
		 */
		std::vector<SymID> list;

		// some forwarded vector interface for convenience:

		void pushBack(SymID id) { list.push_back(id); }

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

		/**
		 * Append another symbol list to this one.
		 */
		void appendList(const SymbolList& other);
	};

	constexpr bool operator==(const SymbolList& lhs, const SymbolList& rhs) {
		return lhs.list == rhs.list;
	}
}
