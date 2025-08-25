#pragma once

#include <base/ref.hpp>
#include <query_framework/context.hpp>
#include <pst_parser/access.hpp>

namespace compiler::frontend {

	class SourceFile;
	struct ModuleID;

	struct FileID final {
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const FileID&) const = default;
		auto operator<=>(const FileID& other) const { return ref.get() <=> other.ref.get(); }

	private:
		base::CRef<SourceFile> ref;

		FileID(base::CRef<SourceFile> ref): ref(ref) {}
		friend class SourceFile;
		friend class ModuleTree;
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend struct ImplementationOf_QueryMainSourceFile;
		friend struct ImplementationOf_QuerySourceFiles;
		friend struct ImplementationOf_QueryFilePST;
		friend ModuleID extendQueryModuleIDOfPST(query::Context& ctx, pst::AccessLocked<pst::LangElement> element);
	};
}
