#pragma once

#include <base/ref.hpp>

namespace compiler::frontend {

	class ModuleTree;

	struct ModuleID final {
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const ModuleID&) const = default;

		auto operator<=>(const ModuleID& other) const { return ref.get() <=> other.ref.get(); }

	private:
		ModuleID(base::CRef<ModuleTree> ref): ref(ref) {}
		base::CRef<ModuleTree> ref;

		friend class ModuleTree;
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend base::StrID moduleName(ModuleID module);
		friend std::string printModuleTree(ModuleID module);
		friend struct ImplementationOf_QueryParentModule;
		friend struct ImplementationOf_QueryMainSourceFile;
		friend struct ImplementationOf_QuerySourceFiles;
		friend struct ImplementationOf_QuerySubmodules;
		friend struct GetModuleID_Functor;
	};
}

namespace std {
    template <>
    struct hash<compiler::frontend::ModuleID> {
        size_t operator()(const compiler::frontend::ModuleID& module_id) const noexcept {
            return static_cast<size_t>(module_id.queryUnstablePerfectHash());
        }
    };
}
