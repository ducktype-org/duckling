#include "utils.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/top_level.hpp>

namespace compiler::repl {

	std::vector<std::string> extractStatementSources(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
		auto pst       = getFilePST(ctx, main_file);
		auto root      = pst->getRootElement().unlock(ctx);

		std::vector<std::string> sources;
		for (auto stmt_locked: root->getStatements()) {
			auto stmt = stmt_locked.unlock(ctx);
			auto pos  = stmt->getSourcePosition();
			sources.emplace_back(
				pos.getSource()->getCharRange(pos.getStart(), pos.getEnd() + 1).stdString()
			);
		}
		return sources;
	}

}  // namespace compiler::repl
