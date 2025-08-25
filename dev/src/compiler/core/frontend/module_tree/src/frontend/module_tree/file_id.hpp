#pragma once

#include <pst_parser/access.hpp>

#include <base/ref.hpp>

#include <query_framework/context.hpp>

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

		FileID(base::Ref<SourceFile> ref): ref(ref) {}

	private:
		base::Ref<SourceFile> ref;
		friend class SourceFile;
		friend class ModuleTree;
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend struct ImplementationOf_QueryMainSourceFile;
		friend struct ImplementationOf_QuerySourceFiles;
		friend struct ImplementationOf_QueryFilePST;
		friend ModuleID extendQueryModuleIDOfPST(
			query::Context& ctx, pst::AccessLocked<pst::LangElement> element
		);
	};
}
