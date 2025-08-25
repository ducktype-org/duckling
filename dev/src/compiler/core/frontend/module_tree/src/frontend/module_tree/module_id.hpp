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

		ModuleID(base::Ref<ModuleTree> ref): ref(ref) {}

	private:
		base::Ref<ModuleTree> ref;

		friend class ModuleTree;
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend base::StrID moduleName(ModuleID module);
		friend std::string printModuleTree(ModuleID module);
		friend struct ImplementationOf_QueryParentModule;
		friend struct ImplementationOf_QueryMainSourceFile;
		friend struct ImplementationOf_QuerySourceFiles;
		friend struct ImplementationOf_QuerySubmodules;
	};
}
