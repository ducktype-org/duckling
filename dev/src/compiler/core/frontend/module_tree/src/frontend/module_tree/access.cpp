#include "access.hpp"
#include "queries.hpp"
#include "module_tree.hpp"
#include "source_file.hpp"
#include "functors.hpp"

#include <query_framework/query_impl.hpp>
#include <query_framework/query_input_impl.hpp>

namespace compiler::frontend {

	IMPLEMENT_QUERY_SIDE_INPUT(QueryModuleSideInput);
	IMPLEMENT_QUERY_SIDE_INPUT(QueryFileSideInput);

	query::QueryStableHash KeyOf_ModuleSideInput::queryStablePerfectHash() const {
		return ModuleTree::getComponentHash(id).hash;
	}

	query::QueryStableHash KeyOf_FileSideInput::queryStablePerfectHash() const {
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
