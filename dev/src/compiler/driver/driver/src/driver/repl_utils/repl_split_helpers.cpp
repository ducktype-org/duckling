// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "repl_split_helpers.hpp"

#include "repl_statement_helpers.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/module_tree_builder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/top_level.hpp>

#include <base/str/str_utils.hpp>

#include <filesystem/file.hpp>
#include <logger/logger.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace compiler::repl {
	std::vector<std::string> extractStatementSources(const pst::TopLevel& root) {
		std::vector<std::string> sources;

		CORE_ASSERT(!query::Context::areWeInsideQuery(), "extractStatementSources must not be called inside a query");

		for (auto stmt_locked: root.getStatements()) {
			auto stmt = stmt_locked.illegalAccess().value();
			auto pos  = stmt->getSourcePosition().illegalAccess();
			sources.emplace_back(
				pos.getSource()->getCharRange(pos.getStart(), pos.getEnd() + 1).stdString()
			);
		}

		return sources;
	}


	namespace {
		/**
		 * @brief Create a minimal ephemeral REPL module for statement splitting.
		 *
		 * Marked as a REPL module (no parent) so that SourceFile::getPST() selects
		 * PSTType::Script, guaranteeing ordered top-level statement parsing.
		 * The returned Ref must stay alive for the duration of any query over the module.
		 *
		 * @note: This is similar to createReplModule, but without parent and history.
		 */
		base::Ref<frontend::ModuleTree> createProbeReplModule(std::string_view input) {
			// @TODO: #2762 build the probe below a script module node, inside a package (a
			// synthetic one if needed), instead of as a root module of a fake package.
			auto builder = frontend::SyntheticReplChainModuleTreeBuilder::create();
			builder->setPackageID(base::StrID("repl_session"));
			builder->setSyntheticSourceFile(fs::FileManager::createRandomVirtualFile(input));
			builder->setName(base::StrID(
				base::strConcat("repl_probe_", std::to_string(ReplSessionID::next().asInt()))
			));

			return builder->finalize();
		}
	}

	// PR: this should not take its ctx, but call conetxt do inside
	std::expected<std::vector<std::string>, std::string> splitInputIntoStatements(
		 std::string_view input
	) {
		auto probe_ref       = createProbeReplModule(input);
		auto probe_module_id = probe_ref->getModuleID();

		base::Optional<CRef<pst::ParsedPST<>>> pst;

		query::utils::withContextDo([&](query::Context& ctx) {
			auto main_file = ctx.query<frontend::QueryMainSourceFile>(probe_module_id);
			pst = getFilePST(ctx, main_file);
		});

		CORE_DEV_LOG(REPL, "PST:\n");
		if (logger::isCategoryEnabled(logger::DevLogCategories::REPL)) {
			pst.value()->dprint(std::cout);
			std::cout << "\n\n";
		}

		if (pst.value()->getLogger()->bad()) return std::unexpected(std::string("Parse error"));

		auto root = pst.value()->getRootElement().illegalAccess().value();
		return extractStatementSources(*root);
	}


}  // namespace compiler::repl
