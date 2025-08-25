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
		base::CRef<ModuleTree> ref;

		ModuleID(base::CRef<ModuleTree> ref): ref(ref) {}
		friend class ModuleTree;
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
	};
}
