#include "validation.hpp"

#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/lsp_ui/lsp_ui.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <helios/queries.hpp>

#include <query_framework/context.hpp>
#include <query_framework/query_entry_point.hpp>

namespace lsp {
	using namespace compiler;

	base::CRef<frontend::ModuleTree> getRootModule(frontend::ModuleID module_id) {
		base::CRef<frontend::ModuleTree> module = frontend::getModuleRef(module_id);
		while (true) {
			auto parent = module->getParentModule();
			if (!parent.has_value()) break;
			module = getModuleRef(parent.value().illegalAccess().getID());
		}
		return module;
	}

	void collectErrorsFromModuleTree(
		base::CRef<frontend::ModuleTree>                  module,
		std::vector<CRef<dia_int::dia_args::Diagnostic>>& out
	) {
		auto main_file
			= frontend::GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module->getMainSourceFile().illegalAccess().getID()
			);
		auto main_pst = main_file->getPST();
		main_pst->getLogger()->collectDiagnostics(out);

		for (const auto& file_ref: module->getSourceFiles()) {
			auto file
				= frontend::GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					file_ref.illegalAccess().getID()
				);
			auto pst = file->getPST();
			pst->getLogger()->collectDiagnostics(out);
		}

		// Recurse into submodules
		for (const auto& submodule_id_locked: module->getSubmodules()) {
			auto submodule = getModuleRef(submodule_id_locked.illegalAccess().getID());
			collectErrorsFromModuleTree(submodule, out);
		}
	}

	bool hasErrorsInModuleTree(base::CRef<frontend::ModuleTree> module) {
		auto main_file
			= frontend::GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module->getMainSourceFile().illegalAccess().getID()
			);
		auto main_pst = main_file->getPST();
		if (main_pst->getLogger()->hasErrors()) return true;

		for (const auto& file_ref: module->getSourceFiles()) {
			auto file
				= frontend::GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					file_ref.illegalAccess().getID()
				);
			auto pst = file->getPST();
			if (pst->getLogger()->hasErrors()) return true;
		}

		// Recurse into submodules
		for (const auto& submodule_id_locked: module->getSubmodules()) {
			auto submodule = getModuleRef(submodule_id_locked.illegalAccess().getID());
			if (hasErrorsInModuleTree(submodule)) return true;
		}
		return false;
	}

	std::vector<CRef<dia_int::dia_args::Diagnostic>> getParserDiagnosticsFromModuleTree(
		base::CRef<frontend::ModuleTree> module
	) {
		std::vector<CRef<dia_int::dia_args::Diagnostic>> diagnostics;
		collectErrorsFromModuleTree(module, diagnostics);
		return diagnostics;
	}

	void jsonSerializeDiagnostics(
		const std::vector<CRef<dia_int::dia_args::Diagnostic>>& diagnostics,
		const dia_int::lsp::EvaluationContext&                  ctx,
		std::ostream&                                           out
	) {
		static base::Optional<base::HashMap<std::string, std::vector<Box<dia_int::lsp::Diagnostic>>>>
			previous_diag_by_file_opt{};

		base::HashMap<std::string, std::vector<Box<dia_int::lsp::Diagnostic>>> diagnostics_by_file;
		for (const auto& diag: diagnostics) {
			dia_int::lsp::LSPDiagnosticResult lsp_diag
				= dia_int::lsp::evaluateToLanguageServerMessage(diag, ctx);

			if (not diagnostics_by_file.contains(lsp_diag.file_uri)) {
				diagnostics_by_file.put(
					lsp_diag.file_uri, std::vector<Box<dia_int::lsp::Diagnostic>>{}
				);
			}
			diagnostics_by_file[lsp_diag.file_uri].push_back(std::move(lsp_diag.diagnostic));
		}

		out << "{\n";
		bool first_list_elem = true;
		for (const auto& [file_uri, diags]: diagnostics_by_file) {
			if (not first_list_elem) out << ",\n";
			first_list_elem = false;

			out << "\"" << file_uri << "\": [\n";
			for (usize i = 0; i < diags.size(); i++) {
				dia_int::lsp::LSPDiagnosticResult::jsonSerializeDiagnostic(diags[i].ref(), out);
				if (i + 1 < diags.size()) out << ",\n";
			}
			out << "]";
			out << "\n";
		}

		// We have to send empty arrays for files that had diagnostics previously
		// but do not have any diagnostics now, to clear them in the client.
		if_opt_some(previous_diag_by_file_opt, previous_diag_by_file) {
			for (const auto& [old_file_uri, old_diags]: previous_diag_by_file) {
				if (not diagnostics_by_file.contains(old_file_uri)) {
					if (not first_list_elem) out << ",\n";
					first_list_elem = false;

					out << "\"" << old_file_uri << "\": []\n";
				}
			}
		}
		out << "}\n";

		previous_diag_by_file_opt = std::move(diagnostics_by_file);
	}

	std::string getDiagnosticJsonFromCompiler(fs::File& file) {
		auto source_files = frontend::SourceFile::getSourceFilesfromFile(file);
		auto module       = source_files[0]->getModule().illegalAccess().getID();
		auto root_module  = getRootModule(module);
		auto main_source_file
			= getFileRef(root_module->getMainSourceFile().illegalAccess().getID());
		auto main_path = main_source_file->getFileIllegalAccess().getFilePath();

		std::vector<CRef<dia_int::dia_args::Diagnostic>> diagnostics
			= getParserDiagnosticsFromModuleTree(root_module);

		if (not hasErrorsInModuleTree(root_module)) {
			query::entryPoint<helios::QueryModuleHOUTRecursively>(root_module->getModuleID());
			query::Context::int_logger.collectDiagnostics(diagnostics);
		}

		auto fs_path_to_uri = [](const std::string& path) {
			auto first = path.find('/');
			if (first == std::string::npos) return "file://" + path;

			auto second = path.find('/', first + 1);
			if (second == std::string::npos) return "file://" + path;

			return "file://" + path.substr(second);
		};

		dia_int::lsp::EvaluationContext ctx(main_path.string(), fs_path_to_uri);

		std::stringstream out;
		jsonSerializeDiagnostics(diagnostics, ctx, out);
		return out.str();
	}
}
