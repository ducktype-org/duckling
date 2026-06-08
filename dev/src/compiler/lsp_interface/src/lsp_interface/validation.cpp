#include "validation.hpp"

#include <diagnostic_interactive/core/common_classes.hpp>
#include <diagnostic_interactive/core/diagnostic_arguments_forward.hpp>
#include <diagnostic_interactive/lsp_ui/lsp_ui.hpp>
#include <diagnostic_interactive/message.hpp>
#include <diagnostic_interactive/stable_position.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <helios/queries/queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <query_framework/context/context_fd.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace lsp {
	using namespace compiler;

	base::CRef<frontend::ModuleTree> getRootModule(frontend::ModuleID module_id) {
		base::CRef<frontend::ModuleTree> module = frontend::getModuleRef(module_id);
		while (auto parent = module->getParentModule())
			module = getModuleRef(parent.value().illegalAccess().getID());
		return module;
	}

	dia_int::CodeLocation updatePositionWithHashCodeLocation(
		dia_int::HashCodeLocation hash_code_location
	) {
		dia_int::StablePosition stable_position(
			pst::LangElement::getActiveSourcePosition,
			pst::LangElement::getActiveSourcePositionIllegalAccess,
			hash_code_location.begin_node,
			hash_code_location.end_node
		);
		auto updated_source_pos = stable_position.getActiveSourcePositionIllegalAccess();
		auto updated_code_location
			= dia_int::CodeLocationArgument::FileLocation::fromSourcePosition(updated_source_pos);
		return updated_code_location.toCodeLocation();
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

		// Recurse into submodules
		for (const auto& submodule_id_locked: module->getSubmodules().illegalAccess()) {
			auto submodule = getModuleRef(submodule_id_locked.illegalAccess().getID());
			collectErrorsFromModuleTree(submodule, out);
		}
	}

	std::vector<CRef<dia_int::dia_args::Diagnostic>> getParserDiagnosticsFromModuleTree(
		base::CRef<frontend::ModuleTree> module
	) {
		std::vector<CRef<dia_int::dia_args::Diagnostic>> diagnostics;
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

	void jsonSerializeDiagnostics(
		const std::vector<CRef<dia_int::dia_args::Diagnostic>>& diagnostics,
		const dia_int::lsp::EvaluationContext&                  ctx,
		std::ostream&                                           out
	) {
		static base::Optional<base::HashMap<base::StrID, std::vector<Box<dia_int::lsp::Diagnostic>>>>
			previous_diag_by_file_opt{};

		base::HashMap<base::StrID, std::vector<Box<dia_int::lsp::Diagnostic>>> diagnostics_by_file;

		// We always want to have at least an entry for the queried file for better experience
		diagnostics_by_file.emplace(
			ctx.queried_file_uri, std::vector<Box<dia_int::lsp::Diagnostic>>{}
		);

		// Iterate over diagnostics and group them by file URI
		for (const auto& diag: diagnostics) {
			dia_int::lsp::LSPDiagnosticResult lsp_diag
				= dia_int::lsp::evaluateToLanguageServerMessage(diag, ctx);
			auto file_uri = base::StrID(lsp_diag.file_uri);

			if (not diagnostics_by_file.contains(file_uri))
				diagnostics_by_file.emplace(file_uri, std::vector<Box<dia_int::lsp::Diagnostic>>{});
			diagnostics_by_file.at(file_uri).push_back(std::move(lsp_diag.diagnostic));
		}


		// Serialize them
		out << "{\n";
		bool first_list_elem = true;
		for (const auto& [file_uri, diags]: diagnostics_by_file) {
			if (not first_list_elem) out << ",\n";
			first_list_elem = false;

			out << "\"" << file_uri.strView() << "\": [\n";
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

					out << "\"" << old_file_uri.strView() << "\": []\n";
				}
			}
		}
		out << "}\n";

		previous_diag_by_file_opt = std::move(diagnostics_by_file);
	}

	std::string getDiagnosticJsonFromCompiler(const fs::File& file) {
		auto source_files = frontend::SourceFile::getSourceFilesFromFile(file);
		if (source_files.empty()) return "{}";

		auto root_module = getRootModule(
			source_files[source_files.size() - 1]->getModule().illegalAccess().getID()
		);
		auto main_source_file
			= getFileRef(root_module->getMainSourceFile().illegalAccess().getID());
		auto main_path    = main_source_file->getFileIllegalAccess().getFilePath().toPhysicalPath();
		auto queried_path = file.getFilePath().toPhysicalPath();

		std::vector<CRef<dia_int::dia_args::Diagnostic>> diagnostics
			= getParserDiagnosticsFromModuleTree(root_module);

		// We run the semantic analysis if there is no parsing errors.

		if (isModuleTreeParsedSuccessfully(root_module)) {
			auto hout
				= query::entryPoint<helios::QueryModuleHOUTRecursively>(root_module->getModuleID());

			// Since MIR lowering can produce errors, we have to run MIR lowering as well.
			if (hout.hasValue()) {
				for (const auto& item: hout.valueOrPanic()) {
					query::utils::withContextDo([&](query::Context& ctx) -> void {
						mir::lowerToMIRUnit(ctx, item);
					});
				}
			}
		}

		query::Context::collectAndUpdateAllDiagnostic(
			diagnostics, updatePositionWithHashCodeLocation
		);

		dia_int::lsp::EvaluationContext ctx(main_path.uri(), queried_path.uri());

		std::stringstream out;
		jsonSerializeDiagnostics(diagnostics, ctx, out);
		return out.str();
	}
}
