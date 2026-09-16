#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/queries/queries.hpp>
#include <lsp_interface/diagnostics.hpp>
#include <lsp_interface/uri_conversion.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <diagnostic/core/common_classes.hpp>
#include <diagnostic/core/diagnostic_arguments_forward.hpp>
#include <diagnostic/lsp_ui/lsp_ui.hpp>
#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>
#include <query_framework/context/context_fd.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <sstream>

namespace duck_ls {
	using namespace compiler;

	namespace {
		/**
		 * @brief The path of a file as it exists on disk, whichever side of the swap it is on.
		 */
		std::string physicalPathString(const fs::FilePath& path) {
			if (path.isVirtual()) return path.toPhysicalPath().string();
			return path.string();
		}

	}

	base::CRef<frontend::ModuleTree> getRootModule(frontend::ModuleID module_id) {
		base::CRef<frontend::ModuleTree> module = frontend::getModuleRef(module_id);
		while (auto parent = module->getParentModule())
			module = getModuleRef(parent.value().illegalAccess().getID());
		return module;
	}

	dia::CodeLocation updatePositionWithHashCodeLocation(dia::HashCodeLocation hash_code_location) {
		dia::StablePosition stable_position(
			pst::LangElement::getActiveSourcePosition,
			pst::LangElement::getActiveSourcePositionIllegalAccess,
			hash_code_location.begin_node,
			hash_code_location.end_node
		);
		auto updated_source_pos = stable_position.getActiveSourcePositionIllegalAccess();
		auto updated_code_location
			= dia::CodeLocationArgument::FileLocation::fromSourcePosition(updated_source_pos);
		return updated_code_location.toCodeLocation();
	}

	void collectErrorsFromModuleTree(
		base::CRef<frontend::ModuleTree> module, std::vector<CRef<dia::dia_args::Diagnostic>>& out
	) {
		auto main_file
			= frontend::GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module->getMainSourceFile().illegalAccess().getID()
			);
		auto main_pst = main_file->getPST();
		main_pst->getLogger()->collectDiagnostics(out);

		// Recurse into submodules
		for (const auto& submodule_id_locked: module->getSubmodules().illegalAccess()) {
			auto submodule = getModuleRef(submodule_id_locked.illegalAccess().getID());
			collectErrorsFromModuleTree(submodule, out);
		}
	}

	std::vector<CRef<dia::dia_args::Diagnostic>> getParserDiagnosticsFromModuleTree(
		base::CRef<frontend::ModuleTree> module
	) {
		std::vector<CRef<dia::dia_args::Diagnostic>> diagnostics;
		collectErrorsFromModuleTree(module, diagnostics);
		return diagnostics;
	}

	bool isModuleTreeParsedSuccessfully(base::CRef<frontend::ModuleTree> module) {
		auto main_file
			= frontend::GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module->getMainSourceFile().illegalAccess().getID()
			);

		if (main_file->getPST()->getLogger()->hasErrors()) return false;

		bool all_submodules_parsed_successfully = true;
		// Recurse into submodules
		for (const auto& submodule_id_locked: module->getSubmodules().illegalAccess()) {
			auto submodule = getModuleRef(submodule_id_locked.illegalAccess().getID());
			all_submodules_parsed_successfully &= isModuleTreeParsedSuccessfully(submodule);
		}
		return all_submodules_parsed_successfully;
	}

	std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> collectDiagnostics(
		const fs::File& file, const OpenDocuments& documents
	) {
		std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> by_file;

		auto source_files = frontend::SourceFile::getSourceFilesFromFile(file);
		if (source_files.empty()) return by_file;

		auto root_module = getRootModule(
			source_files[source_files.size() - 1]->getModule().illegalAccess().getID()
		);
		auto main_source_file
			= getFileRef(root_module->getMainSourceFile().illegalAccess().getID());

		auto queried_path = file.getFilePath();
		auto main_path    = main_source_file->getFileIllegalAccess().getFilePath();

		std::vector<CRef<dia::dia_args::Diagnostic>> diagnostics
			= getParserDiagnosticsFromModuleTree(root_module);

		// Semantic analysis only runs once the whole package parses.
		if (isModuleTreeParsedSuccessfully(root_module)) {
			query::utils::withContextDo([&](query::Context& ctx) -> void {
				auto hout
					= ctx.query<helios::QueryModuleHOUTRecursively>(root_module->getModuleID());

				// MIR lowering can produce errors, so it has to run as well.
				if (hout.hasValue())
					for (const auto& item: hout.valueOrPanic()) mir::lowerToMIRUnit(ctx, item);
			});
		}

		query::Context::collectAndUpdateAllDiagnostic(
			diagnostics, updatePositionWithHashCodeLocation
		);

		dia::lsp::EvaluationContext ctx(
			physicalPathString(main_path),
			physicalPathString(queried_path),
			[&documents](const std::string& path) {
				return toDocumentUri(fs::FilePath(path), documents);
			}
		);

		// The queried file always gets an entry, so that fixing its last error clears it.
		by_file.emplace(ctx.resolve(ctx.queried_file), std::vector<lsp::Diagnostic>{});

		for (const auto& diag: diagnostics) {
			dia::lsp::LSPDiagnosticResult result
				= dia::lsp::evaluateToLanguageServerMessage(diag, ctx);
			by_file[result.uri].push_back(std::move(result.diagnostic));
		}

		return by_file;
	}

	void publishDiagnostics(
		lsp::ServerEndpoint& endpoint, ServerSession& session, const fs::FilePath& path
	) {
		auto cache_twin = session.cachePath(path);
		auto file       = cache_twin.exists() ? fs::File(cache_twin) : fs::File(path);

		auto published = collectDiagnostics(file, session.documents);

		// Files that had diagnostics before and have none now must be cleared explicitly.
		for (const auto& [uri, previous]: session.last_published_diagnostics)
			if (!previous.empty() && !published.contains(uri))
				published.emplace(uri, std::vector<lsp::Diagnostic>{});

		for (const auto& [uri, diagnostics]: published)
			endpoint.textDocumentPublishDiagnostics({ .uri = uri, .diagnostics = diagnostics });

		session.last_published_diagnostics = std::move(published);
	}
}
