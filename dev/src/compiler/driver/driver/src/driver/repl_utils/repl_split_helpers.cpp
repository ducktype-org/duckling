#include "repl_split_helpers.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/top_level.hpp>

#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace compiler::repl {
	std::vector<std::string> extractStatementSources(query::Context& ctx, const pst::TopLevel& root) {
		std::vector<std::string> sources;
		for (auto stmt_locked: root.getStatements()) {
			auto stmt = stmt_locked.unlock(ctx);
			auto pos  = stmt->getSourcePosition();
			sources.emplace_back(
				pos.getSource()->getCharRange(pos.getStart(), pos.getEnd() + 1).stdString()
			);
		}

		return sources;
	}

	std::vector<std::string> extractStatementSources(
		query::Context& ctx, frontend::ModuleID module_id
	) {
		auto main_file = ctx.query<frontend::QueryMainSourceFile>(module_id);
		auto pst       = getFilePST(ctx, main_file);
		auto root      = pst->getRootElement().unlock(ctx);
		return extractStatementSources(ctx, *root);
	}

	/**
	 * @brief Create a minimal ephemeral REPL module for statement splitting.
	 *
	 * Marked as a REPL module (no parent) so that SourceFile::getPST() selects
	 * PSTType::Script, guaranteeing ordered top-level statement parsing.
	 * The returned Ref must stay alive for the duration of any query over the module.
	 *
	 * @note: This is similar to createReplModule, but without parent and history.
	 */
	static base::Ref<frontend::ModuleTree> createProbeReplModule(std::string_view input) {
		auto builder = frontend::ModuleTreeBuilder::create();
		builder->setPackageID(base::generateRandomString(32));
		builder->setMainSourceFile(fs::FileManager::createRandomVirtualFile(input));
		builder->setName(base::StrID("repl_probe"));
		builder->setReplModule(frontend::ReplData{});
		return builder->finalize();
	}

	std::expected<std::vector<std::string>, std::string> splitInputIntoStatements(
		query::Context& ctx, std::string_view input
	) {
		auto probe_ref       = createProbeReplModule(input);
		auto probe_module_id = probe_ref->getModuleID();

		auto main_file = ctx.query<frontend::QueryMainSourceFile>(probe_module_id);
		auto pst       = getFilePST(ctx, main_file);

		CORE_DEV_LOG(REPL, "PST:\n");
		if (logger::isCategoryEnabled(logger::DevLogCategories::REPL)) {
			pst->dprint(std::cout);
			std::cout << "\n\n";
		}

		if (pst->getLogger()->bad()) {
			std::cerr << "Parse errors:\n";
			pst->getLogger()->dumpLog(false, std::cerr);
			return std::unexpected(std::string("Parse error"));
		}

		auto root = pst->getRootElement().unlock(ctx);
		return extractStatementSources(ctx, *root);
	}

	std::expected<std::vector<std::string>, std::string> splitInputIntoStatements(
		std::string_view input
	) {
		std::expected<std::vector<std::string>, std::string> result;

		query::utils::withContextDo([&](query::Context& ctx) {
			result = splitInputIntoStatements(ctx, input);
		});

		return result;
	}

}  // namespace compiler::repl
