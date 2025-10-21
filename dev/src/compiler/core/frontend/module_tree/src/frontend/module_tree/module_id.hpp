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
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const ModuleID&) const = default;

	private:
		ModuleID(base::Ref<ModuleTree> ref): ref(ref) {}

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
