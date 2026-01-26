#include "access.hpp"

#include "functors.hpp"
#include "module_tree.hpp"
#include "queries.hpp"
#include "source_file.hpp"

#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/input_query/query_input_impl.hpp>

namespace compiler::frontend {

	IMPLEMENT_QUERY_SIDE_INPUT(QueryModuleSideInput);
	IMPLEMENT_QUERY_SIDE_INPUT(QueryFileSideInput);

	query::QueryStableHash KeyOf_ModuleSideInput::queryStablePerfectHash() const {
		return ModuleTree::getModuleHash(id);
	}

	query::QueryStableHash KeyOf_FileSideInput::queryStablePerfectHash() const {
		// We can youse file component hash for stable hash of FileID
		// This is because we can only get from file 1) Its parent module 2) Its name
		// Both are included in component hash
		// The PST Tree has its own access side input for its content
		// Also: Every SourceFile has a PST associated with it
		// This might change in the future but for now we don't need much from a SourceFile itself
		return getFileRef(id)->getComponentHash().hash;
	}

	template<>
	ModuleAccess ModuleAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryModuleSideInput>(KeyOf_ModuleSideInput{ id });
		return ModuleAccess(id);
	}

	template<>
	FileAccess FileAccessLocked::unlock(query::Context& ctx) const {
		ctx.query<QueryFileSideInput>(KeyOf_FileSideInput{ id });
		return FileAccess(id);
	}

}
