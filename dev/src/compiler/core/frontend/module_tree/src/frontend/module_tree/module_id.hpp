// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>
#include <base/types/bit256.hpp>

namespace compiler::frontend {

	class ModuleTree;

	/**
	 * @brief ModuleID is a unique identifier for a ModuleTree in the Duckling compiler.
	 *
	 * ModuleID wraps a reference to a ModuleTree and provides hashing and identity.
	 * It is used to track and query modules in the module tree structure during the compilation
	 * process. ModuleID is not copy-constructible from outside; use functors or friend classes to
	 * create instances.
	 */
	struct ModuleID final {
		// @TODO: #1389 queryUnstablePerfectHash and `==` are inconsistent with the module tree stable
		// hash. For now it should work, but might break in the future. Decide what to do about it.

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const {
			return hash;
		}

		bool operator==(const ModuleID&) const = default;

		auto operator<=>(const ModuleID&) const = default;

	private:
		/**
		 * @brief Creates the ID from the current hash of the module and registers the module under
		 * that hash.
		 */
		ModuleID(base::Ref<ModuleTree> ref);

		/**
		 * @brief Resolves the ID to the module currently registered under its hash.
		 * In development builds it panics when that module was removed from storage.
		 */
		[[nodiscard]] base::Ref<ModuleTree> resolve() const;

		base::Bit256 hash;

		friend class ModuleTree;
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend struct GetModuleID_Functor;
	};
}

namespace std {
	template<>
	struct hash<compiler::frontend::ModuleID> {
		size_t operator()(const compiler::frontend::ModuleID& module_id) const noexcept {
			return std::hash<base::Bit256>{}(module_id.queryUnstablePerfectHash());
		}
	};
}
