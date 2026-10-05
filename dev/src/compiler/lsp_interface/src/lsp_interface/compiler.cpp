// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <driver/incremental_utils/collect_input.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/module_tree_builder.hpp>
#include <frontend/module_tree/module_tree_modifier.hpp>
#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>
#include <frontend/pst_parser/source_position_locked.hpp>
#include <global_state/packages.hpp>
#include <helios/queries/queries.hpp>
#include <lsp_interface/compiler.hpp>
#include <mir/mir_lowering/mir_unit.hpp>

#include <diagnostic/core/common_classes.hpp>
#include <diagnostic/core/diagnostic_arguments_forward.hpp>
#include <diagnostic/lsp_ui/lsp_ui.hpp>
#include <diagnostic/message.hpp>
#include <diagnostic/stable_position.hpp>
#include <query_framework/context/context_fd.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <query_framework/external/api.hpp>

#include <cctype>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace duck_ls {

	using namespace compiler;
	using namespace compiler::frontend;

	namespace {
		bool hasDirectoryMainModuleFile(const fs::FilePath& dir_path) {
			if (!dir_path.exists()) return false;
			return dir_path.join(dir_path.name() + std::string(LANG_MODULE_FILE)).exists();
		}

		void collectInputsFor(const fs::File& file, std::vector<query::external::InputData>& out) {
			for (auto& source_file: SourceFile::getSourceFilesFromPath(file.getFilePath()))
				compiler::driver::collectQueryInputsFromPst(source_file->getPST(), out);
		}

		base::Optional<base::Ref<ModuleTree>> findModuleForFile(const fs::File& file) {
			auto source_files = SourceFile::getSourceFilesFromPath(file.getFilePath());
			if (source_files.empty()) return {};

			auto module_id = source_files.back()->getModule().illegalAccess().getID();
			return GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module_id
			);
		}

		base::CRef<ModuleTree> getRootModule(ModuleID module_id) {
			base::CRef<ModuleTree> module = getModuleRef(module_id);
			while (auto parent = module->getParentModule())
				module = getModuleRef(parent.value().illegalAccess().getID());
			return module;
		}

		base::Optional<base::CRef<ModuleTree>> findLoadedPackageOf(const fs::FilePath& path) {
			auto source_files = SourceFile::getSourceFilesFromPath(path);
			if (source_files.empty()) return {};

			return getRootModule(source_files.back()->getModule().illegalAccess().getID());
		}

		dia::CodeLocation updatePositionWithHashCodeLocation(dia::HashCodeLocation hash_code_location
		) {
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
			base::CRef<ModuleTree> module, std::vector<CRef<dia::dia_args::Diagnostic>>& out
		) {
			auto main_file
				= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					module->getMainSourceFile().illegalAccess().getID()
				);
			main_file->getPST()->getLogger()->collectDiagnostics(out);

			for (const auto& submodule_id_locked: module->getSubmodules().illegalAccess())
				collectErrorsFromModuleTree(
					getModuleRef(submodule_id_locked.illegalAccess().getID()), out
				);
		}

		bool isModuleTreeParsedSuccessfully(base::CRef<ModuleTree> module) {
			auto main_file
				= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					module->getMainSourceFile().illegalAccess().getID()
				);

			if (main_file->getPST()->getLogger()->hasErrors()) return false;

			bool all_submodules_parsed_successfully = true;
			for (const auto& submodule_id_locked: module->getSubmodules().illegalAccess())
				all_submodules_parsed_successfully &= isModuleTreeParsedSuccessfully(
					getModuleRef(submodule_id_locked.illegalAccess().getID())
				);
			return all_submodules_parsed_successfully;
		}
	}

	/**
	 * @brief Create a package ID from the path of the package root.
	 *
	 * @param package_root
	 * @return base::StrID
	 */
	base::StrID packageIdForRoot(const fs::FilePath& package_root) {
		std::string name;
		for (const char c: package_root.name())
			name += std::isalnum(static_cast<unsigned char>(c)) != 0 ? c : '_';

		std::ostringstream id;
		id << name << '_' << std::hex << std::setw(16) << std::setfill('0')
		   << std::hash<std::string>{}(package_root.absolute().genericString());

		return base::StrID(id.str());
	}

	Compiler::Compiler(base::Ref<ServerSession> session, base::Ref<FilesCache> files):
		  session(session),
		  files(files) {}

	void Compiler::addWorkspace(const lsp::Uri& root) {
		if_opt_some(files->sourcePathFor(root), path) files->addWorkspaceRoot(path);
	}

	base::Optional<fs::FilePath> Compiler::walkToPackageRoot(const fs::FilePath& path) {
		auto workspace_root = files->workspaceRootFor(path);
		if (workspace_root.empty()) return {};

		base::Optional<fs::FilePath> package_root;

		for (auto current = path.parentPath();; current = current.parentPath()) {
			if (!hasDirectoryMainModuleFile(current)) break;
			package_root = current;
			if (current == workspace_root.value()) break;
			// The filesystem root is its own parent, so it has to end the walk explicitly.
			if (current == current.parentPath()) break;
		}

		return package_root;
	}

	void Compiler::loadPackage(const fs::FilePath& package_root) {
		auto resolver = [this](const fs::File& source_file) -> fs::File {
			auto source_path = source_file.getFilePath();
			if (files->isOpened(source_path)) return { files->cachePath(source_path) };
			return source_file;
		};

		auto module_id = ModuleTreeBuilder::create(
							 fs::File(package_root), packageIdForRoot(package_root), resolver
		)
		                     ->getModuleID();
		global_state::setters::addPackage(module_id);
		tracked_packages.push_back(module_id);

		std::cerr << "duck_ls: loaded package " << package_root.strView() << "\n";
	}

	void Compiler::reloadPackageOwning(const fs::FilePath& path) {
		// @TODO: #3607 fix when main package file is deleted or added
		auto package_root = walkToPackageRoot(path);
		if (package_root.empty()) return;

		// Tear the package down and walk it again: rebuilding is affordable for watcher events,
		// and incremental structural edits are where the subtle bugs live.
		auto root_module_file
			= package_root.value().join(package_root.value().name() + std::string(LANG_MODULE_FILE));
		auto root_file = files->isOpened(root_module_file)
		                   ? fs::File(files->cachePath(root_module_file))
		                   : fs::File(root_module_file);

		if_opt_some(findModuleForFile(root_file), module_ref) {
			global_state::setters::removePackage(module_ref->getModuleID());
			ModuleTreeModifier::removeModuleRecursive(module_ref);
			std::erase(tracked_packages, module_ref->getModuleID());
		}

		loadPackage(package_root.value());

		auto new_inputs = compiler::driver::collectInputDataFromGlobalPackagesFromCurrentMetadata();
		query::external::invalidateQueries(std::move(new_inputs), {}, {});
	}

	void Compiler::swapMainSourceFile(
		base::Ref<ModuleTree> module, const fs::File& current, const fs::File& replacement
	) {
		// The old PST must still exist while its inputs are collected.
		std::vector<query::external::InputData> previous_inputs;
		previous_inputs.push_back(pst::SourcePositionLocked::getQueryInputNode());
		collectInputsFor(current, previous_inputs);

		ModuleTreeModifier::removeMainSourceFile(module);
		ModuleTreeModifier::setMainSourceFile(module, replacement);

		std::vector<query::external::InputData> new_inputs;
		collectInputsFor(replacement, new_inputs);

		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});
	}

	void Compiler::openDocument(
		const lsp::Uri& uri, std::string_view language_id, i32 version, std::string_view text
	) {
		auto cache_path = files->openDocument(uri, language_id, version, text);
		if (cache_path.empty()) {
			std::cerr << "duck_ls: unsupported document URI: " << uri.toString() << "\n";
			return;
		}

		auto source = files->sourcePath(cache_path.value());

		if (!source.isRegularFile()) {
			std::cerr << "duck_ls: a document with no file behind it is not supported yet: "
					  << uri.toString() << "\n";
			return;
		}

		auto source_file    = fs::File(source);
		auto module_ref_opt = findModuleForFile(source_file);

		if_opt_some(module_ref_opt, module_ref) {
			swapMainSourceFile(module_ref, source_file, fs::File(cache_path.value()));
			return;
		}
		if_opt_none(module_ref_opt) {
			if (walkToPackageRoot(source).empty()) {
				std::cerr << "duck_ls: no package root for " << source.strView() << "\n";
				return;
			}

			reloadPackageOwning(source);
		}
	}

	base::OkBad Compiler::updateDocument(
		const lsp::Uri&                                        uri,
		i32                                                    version,
		const lsp::Array<lsp::TextDocumentContentChangeEvent>& changes
	) {
		auto cache_path = files->cachePathFor(uri);
		if (cache_path.empty() || !cache_path.value().exists()) {
			std::cerr << "duck_ls: change for a document with no buffer: " << uri.toString()
					  << "\n";
			return base::BAD;
		}

		auto file = fs::File(cache_path.value());

		std::vector<query::external::InputData> previous_inputs;
		previous_inputs.push_back(pst::SourcePositionLocked::getQueryInputNode());
		collectInputsFor(file, previous_inputs);

		if (files->updateDocument(uri, version, changes).isBad()) return base::BAD;

		ModuleTreeModifier::fileModified(file);

		std::vector<query::external::InputData> new_inputs;
		collectInputsFor(file, new_inputs);

		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});
		return base::OK;
	}

	base::OkBad Compiler::closeDocument(const lsp::Uri& uri) {
		auto cache_path = files->cachePathFor(uri);
		if (cache_path.empty() || !files->isOpened(uri)) return base::BAD;

		auto cache_file = fs::File(cache_path.value());
		auto source     = files->sourcePath(cache_path.value());

		if (!source.isRegularFile()) {
			files->closeDocument(uri);
			reloadPackageOwning(source);
			return base::OK;
		}

		if_opt_some(findModuleForFile(cache_file), module_ref) {
			swapMainSourceFile(module_ref, cache_file, source);
		}

		files->closeDocument(uri);
		return base::OK;
	}

	void Compiler::fileCreatedOrDeletedOnDisk(const lsp::Uri& uri) {
		if_opt_some(files->sourcePathFor(uri), source) reloadPackageOwning(source);
	}

	void Compiler::publishDiagnostics(const base::Optional<lsp::Uri>& queried_uri_opt) {
		base::Optional<fs::FilePath>        queried_file_opt;
		std::vector<base::CRef<ModuleTree>> modules;

		if_opt_some(queried_uri_opt, uri) {
			auto cache_path = files->cachePathFor(uri);
			// Cache path is invalid only if and only if the URI type is not supported.
			if (cache_path.empty()) return;

			queried_file_opt
				= files->isOpened(uri) ? cache_path.value() : files->sourcePath(cache_path.value());

			if_opt_some(findLoadedPackageOf(queried_file_opt.value()), root_module)
				modules.push_back(root_module);
		}
		if_opt_none(queried_file_opt) {
			for (auto id: tracked_packages) modules.push_back(getModuleRef(id));
		}

		std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> published;

		std::vector<CRef<dia::dia_args::Diagnostic>> collected;
		base::Optional<fs::FilePath>                 main_path_opt;

		for (auto root_module: modules) {
			auto main_source_file
				= getFileRef(root_module->getMainSourceFile().illegalAccess().getID());
			if_opt_none(main_path_opt) main_path_opt
				= main_source_file->getFileIllegalAccess().getFilePath();

			collectErrorsFromModuleTree(root_module, collected);

			// Semantic analysis only runs once the whole package parses.
			if (isModuleTreeParsedSuccessfully(root_module))
				query::utils::withContextDo([&](query::Context& ctx) -> void {
					auto hout
						= ctx.query<helios::QueryModuleHOUTRecursively>(root_module->getModuleID());

					// MIR lowering can produce errors, so it has to run as well.
					if (hout.hasValue())
						for (const auto& item: hout.valueOrPanic()) mir::lowerToMIRUnit(ctx, item);
				});
		}

		if_opt_some(main_path_opt, main_path) {
			query::Context::collectAndUpdateAllDiagnostic(
				collected, updatePositionWithHashCodeLocation
			);

			dia::lsp::EvaluationContext ctx(
				main_path.string(),
				queried_file_opt.copyValueOr(main_path).genericString(),
				[this](const std::string& path) { return files->uriFor(fs::FilePath(path)); }
			);

			// The queried file always gets an entry, so that fixing its last error clears it.
			published.emplace(ctx.resolve(ctx.queried_file), std::vector<lsp::Diagnostic>{});

			for (const auto& diagnostic: collected) {
				dia::lsp::LSPDiagnosticResult result
					= dia::lsp::evaluateToLanguageServerMessage(diagnostic, ctx);
				published[result.uri].push_back(std::move(result.diagnostic));
			}
		}

		// Files that had diagnostics before and have none now must be cleared explicitly.
		for (const auto& [previous_uri, previous]: last_published_diagnostics)
			if (!previous.empty() && !published.contains(previous_uri))
				published.emplace(previous_uri, std::vector<lsp::Diagnostic>{});

		for (const auto& [published_uri, diagnostics]: published)
			session->pushDiagnostics(published_uri, diagnostics);

		last_published_diagnostics = std::move(published);
	}

}
