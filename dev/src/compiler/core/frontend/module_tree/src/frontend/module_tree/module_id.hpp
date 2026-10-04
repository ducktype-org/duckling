// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/pointers/ref.hpp>

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
		u64 queryUnstablePerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const ModuleID&) const = default;

	private:
		ModuleID(base::Ref<ModuleTree> ref): ref(ref) {}

		/**
		 * @brief Ensures the referenced SourceFile is still valid during development builds.
		 * This function will work only if use_module_modifier_remove is enabled.
		 */
		void checkDanglingReference() const;

		base::Ref<ModuleTree> ref;

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
			return static_cast<size_t>(module_id.queryUnstablePerfectHash());
		}
	};
}
