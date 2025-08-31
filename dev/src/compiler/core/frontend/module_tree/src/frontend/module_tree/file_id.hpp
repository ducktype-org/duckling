#pragma once

#include <pst_parser/access.hpp>

#include <base/ref.hpp>

#include <query_framework/context.hpp>

namespace compiler::frontend {

	class SourceFile;
	struct ModuleID;

	/**
	 * @brief FileID is a unique identifier for a SourceFile in the Duckling compiler.
	 *
	 * FileID wraps a reference to a SourceFile and provides hashing.
	 * It is used to track and query source files in module trees.
	 * FileID is not copy-constructible from outside; use functors or friend classes to create
	 * instances.
	 */
	struct FileID final {
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const {
			return reinterpret_cast<u64>(ref.get());
		}

		bool operator==(const FileID&) const = default;

		auto operator<=>(const FileID& other) const { return ref.get() <=> other.ref.get(); }

	private:
		FileID(base::Ref<SourceFile> ref): ref(ref) {}

		base::Ref<SourceFile> ref;
		friend class SourceFile;
		friend class ModuleTree;
		friend class ModuleTreeBuilder;
		friend class ModuleTreeModifier;
		friend struct ImplementationOf_QueryMainSourceFile;
		friend struct ImplementationOf_QuerySourceFiles;
		friend struct ImplementationOf_QueryFilePST;
		friend struct GetFileID_Functor;
		friend ModuleID extendQueryModuleIDOfPST(
			query::Context& ctx, pst::AccessLocked<pst::LangElement> element
		);
	};
}
