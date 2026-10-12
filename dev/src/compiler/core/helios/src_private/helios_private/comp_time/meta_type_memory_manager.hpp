// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <helios/tsh/symbol_type.hpp>

#include <base/collections/maps.hpp>
#include <base/pointers/box.hpp>

namespace compiler::helios::comptime_ops {
	/**
	 * @brief Singleton memory manager for heap-allocated SymbolTypes created during CTE.
	 *
	 * Additionally this class provides caching if heap allocated types. So if two of the same
	 * SymbolTypes are requested during CTE, only one of them will exist on the heap. It ensures that
	 *
	 * The main purpose is to provide stable, raw pointers to `SymbolType` objects that can be
	 * passed to the DVM as `opaque_ptr`. Since the VM does not own these objects,
	 * this manager retains ownership and ensures they stay alive until `clearAll()` is called.
	 */
	class MetaTypeMemoryManager {
	private:
		// Wrapper struct since hash function must be copy constructible.
		struct SymbolTypeHasher {
			std::size_t operator()(const tsh::SymbolType<>& t) const {
				return std::hash<decltype(t.queryUnstablePerfectHash())>{}(
					t.queryUnstablePerfectHash()
				);
			}
		};

		base::HashMap<tsh::SymbolType<>, Box<tsh::SymbolType<>>, SymbolTypeHasher> allocated_types;
		MetaTypeMemoryManager() = default;

	public:
		static MetaTypeMemoryManager& instance() {
			static MetaTypeMemoryManager manager;
			return manager;
		}

		MetaTypeMemoryManager(const MetaTypeMemoryManager&)            = delete;
		MetaTypeMemoryManager& operator=(const MetaTypeMemoryManager&) = delete;

		/**
		 * @brief Returns the pointer to a given tsh::SymbolType<> and takes ownership of it.
		 * If a given `tsh::SymbolType<>` was already allocated, a pointer to the previous instance
		 * is returned. Otherwise we heap allocate it and store it in the cache.
		 *
		 * @param type The `tsh::SymbolType<>` value to allocate/find in cache.
		 * @return A raw pointer to the allocated SymbolType.
		 */
		tsh::SymbolType<>* allocateType(const tsh::SymbolType<>& type) {
			auto maybe_cached = allocated_types.atMaybe(type);
			if (maybe_cached.has_value()) return maybe_cached.value()->get();

			auto               type_box = base::makeBox<tsh::SymbolType<>>(type);
			tsh::SymbolType<>* raw_ptr  = type_box.get();
			allocated_types.put(type, std::move(type_box));
			return raw_ptr;
		}
	};
}
