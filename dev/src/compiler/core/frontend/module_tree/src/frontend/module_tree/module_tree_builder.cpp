// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_tree_builder.hpp"

#include "functors.hpp"
#include "standard_module_tree.hpp"
#include "module_tree.hpp"
#include "module_tree_modifier.hpp"
#include "script_module_tree.hpp"
#include "source_file.hpp"

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <string_id/string_id.hpp>

#include <regex>
#include <string>

namespace {
	/**
	 * Checks if a file name is valid according to the reject regex.
	 * @param filename The file name to check.
	 * @param reject_file_regex The regex to use for rejection.
	 * @return True if valid, false otherwise.
	 */
	bool isFileNameValid(base::StrID filename, const std::regex& reject_file_regex) {
		auto view = filename.strView();
		return !std::regex_match(view.begin(), view.end(), reject_file_regex);
	}

	/**
	 * Checks if a directory name is valid according to the reject regex.
	 * @param dirname The directory name to check.
	 * @param reject_directory_regex The regex to use for rejection.
	 * @return True if valid, false otherwise.
	 */
	bool isDirectoryNameValid(base::StrID dirname, const std::regex& reject_directory_regex) {
		auto view = dirname.strView();
		return !std::regex_match(view.begin(), view.end(), reject_directory_regex);
	}
}

namespace compiler::frontend {

	Ref<ModuleTree> StandardModuleTreeBuilder::create(
		const fs::File&     root,
		base::StrID         package_id,
		const FileResolver& file_resolver,
		const std::regex&   file_reject,
		const std::regex&   dir_reject
	) {
		base::Box<StandardModuleTreeBuilder> builder = StandardModuleTreeBuilder::create();

		if (root.isDirectory())
			builder->buildFromDirectory(root, package_id, file_resolver, file_reject, dir_reject);
		else
			builder->buildFromSingleFile(root, package_id);

		return builder->finalize();
	}

	Ref<ModuleTree> StandardModuleTreeBuilder::createWithRandomPackageID(
		const fs::File& root, const std::regex& file_reject, const std::regex& dir_reject
	) {
		return create(
			root,
			base::StrID(base::generateRandomString(32)),
			identityFileResolver(),
			file_reject,
			dir_reject
		);
	}

	void StandardModuleTreeBuilder::buildFromDirectory(
		const fs::File&     directory,
		base::StrID         package_id,
		const FileResolver& file_resolver,
		const std::regex&   file_reject,
		const std::regex&   dir_reject
	) {
		CORE_ASSERT(
			directory.isDirectory(),
			base::strConcat("Expected directory, got file: ", directory.getFilePath().string())
		);

		setName(base::StrID(directory.name().c_str()));
		setPackageID(package_id);

		// Process all files and directories in the current directory
		for (const auto& path: directory.listFilePaths()) {
			// Skip symlinks to avoid cycles
			if (path.isSymlink()) continue;

			fs::File file(path);

			if (file.isDirectory()) {
				// Handle subdirectory
				if (!isDirectoryNameValid(base::StrID(file.name()), dir_reject)) continue;

				// build sub-module from directory
				base::Box<StandardModuleTreeBuilder> submodule_builder
					= StandardModuleTreeBuilder::create();
				submodule_builder->buildFromDirectory(
					file, package_id, file_resolver, file_reject, dir_reject
				);
				auto submodule = submodule_builder->finalize();
				CORE_ASSERT(
					submodule->getName() == base::StrID(file.name()), "Submodule name does not match"
				);

				// Discards directories without main module file:
				// Note: it is not decided yet if this behavior is desirable
				if (submodule->hasMainSourceFile()) addSubmodule(submodule);
			} else {
				file = file_resolver(file);
				CORE_ASSERT(
					file.isFile(),
					base::strConcat("File resolver substituted a directory for: ", path.string())
				);

				// Handle regular file
				if (!isFileNameValid(base::StrID(file.name()), file_reject)) continue;
				handleNewFile(file);
			}
		}
	}

	void StandardModuleTreeBuilder::buildFromSingleFile(const fs::File& file, base::StrID package_id) {
		base::StrID stem      = base::StrID(file.stem());
		std::string extension = file.extension();
		CORE_ASSERT(
			extension == LANG_MODULE_FILE,
			"Expected a module file, got: " + file.getFilePath().string()
		);
		setName(stem);
		setPackageID(package_id);
		setMainSourceFile(file);
	}

	void StandardModuleTreeBuilder::handleNewFile(const fs::File& file) {
		CORE_ASSERT(
			file.isFile(),
			base::strConcat("Expected file, got directory: ", file.getFilePath().string())
		);
		std::string stem      = file.stem();
		std::string extension = file.extension();

		if (extension == LANG_MODULE_FILE) {
			// Module file
			auto stem_id = base::StrID(stem);

			if (stem_id == m_name) {
				setMainSourceFile(file);
			} else {
				base::Box<StandardModuleTreeBuilder> submodule_builder
					= StandardModuleTreeBuilder::create();
				submodule_builder->buildFromSingleFile(file, this->m_package_id);
				auto submodule = submodule_builder->finalize();
				CORE_ASSERT(submodule->getName() == stem_id, "Submodule name does not match");
				addSubmodule(base::Ref<ModuleTree>(submodule));
			}
		} else if (extension == LANG_SCRIPT_FILE) {
			// @TODO: #2762 handle script files
			// mock for now:
			addOtherFile(file);
		} else {
			// Other file
			addOtherFile(file);
		}
	}

	/*********************
	 * ModuleTreeBuilderCommon
	 *********************/

	bool ModuleTreeBuilderCommon::isFinalized() const { return m_finalized; }

	void ModuleTreeBuilderCommon::assertNotFinalized() const {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
	}

	void ModuleTreeBuilderCommon::setName(base::StrID name) {
		assertNotFinalized();
		CORE_ASSERT(m_name.isBad(), "Module name is already set");
		m_name = name;
	}

	void ModuleTreeBuilderCommon::setPackageID(base::StrID package_id) {
		assertNotFinalized();
		CORE_ASSERT(m_package_id.isBad(), "Package ID is already set");
		m_package_id = package_id;
	}

	void ModuleTreeBuilderCommon::setParentCommon(base::Ref<ModuleTree> parent) {
		assertNotFinalized();
		CORE_ASSERT(!m_parent.has_value(), "Parent module is already set");
		m_parent = parent;
	}

	base::Ref<ModuleTree> ModuleTreeBuilderCommon::finalizeCommon(base::Box<ModuleTree> node) {
		assertNotFinalized();
		CORE_ASSERT(m_name.isGood(), "Module name must be set before finalizing the module tree");
		CORE_ASSERT(
			m_package_id.isGood(), "Package ID must be set before finalizing the module tree"
		);

		m_finalized = true;

		Ref<ModuleTree> module_ref = ModuleTree::addModuleToStorage(std::move(node));
		module_ref->m_self         = module_ref;
		module_ref->m_name         = m_name;
		module_ref->m_package_id   = m_package_id;
		return module_ref;
	}

	/*********************
	 * StandardModuleTreeBuilder
	 *********************/

	base::Box<StandardModuleTreeBuilder> StandardModuleTreeBuilder::create() {
		return base::makeBox<StandardModuleTreeBuilder>(StandardModuleTreeBuilder());
	}

	base::Box<StandardModuleTreeBuilder> StandardModuleTreeBuilder::createWithRandomPackageID() {
		base::Box<StandardModuleTreeBuilder> builder = StandardModuleTreeBuilder::create();
		builder->setPackageID(base::StrID(base::generateRandomString(32)));
		return builder;
	}

	void StandardModuleTreeBuilder::setMainSourceFile(const fs::File& file) {
		assertNotFinalized();
		CORE_ASSERT(!m_main_source_file_path.has_value(), "Main source file already set");
		m_main_source_file_path = file;
	}

	void StandardModuleTreeBuilder::addSubmodule(base::Ref<ModuleTree> submodule) {
		assertNotFinalized();
		CORE_ASSERT(
			submodule->getKind() == ModuleKind::Module or submodule->getKind() == ModuleKind::Script,
			"Only standard and script modules can be submodules"
		);
		CORE_ASSERT(
			!m_submodules.contains(submodule->getName()),
			"Submodule with the same name already added"
		);
		m_submodules.put(submodule->getName(), submodule);
	}

	void StandardModuleTreeBuilder::addOtherFile(const fs::File& file) {
		assertNotFinalized();
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		if (!m_other_files.contains(ext_id)) m_other_files.put(ext_id, std::vector<fs::File>());
		m_other_files.at(ext_id).push_back(file);
	}

	void StandardModuleTreeBuilder::setParent(base::Ref<ModuleTree> parent) {
		CORE_ASSERT(
			parent->getKind() == ModuleKind::Module, "Parent module must be a standard module"
		);
		setParentCommon(parent);
	}

	base::Ref<ModuleTree> StandardModuleTreeBuilder::finalize() {
		Ref<ModuleTree> module_ref = finalizeCommon(makeBox<StandardModuleTreeNode>());
		auto            node       = module_ref->as<StandardModuleTreeNode>();

		node->m_other_files = std::move(m_other_files);

		if (m_parent.has_value()) StandardModuleTreeModifier::setParent(module_ref, m_parent.value());

		// Create SourceFiles from stored paths
		if (m_main_source_file_path.has_value())
			node->m_main_source_file
				= SourceFile::create(m_main_source_file_path.value(), module_ref);

		for (const auto& [name, submodule]: m_submodules)
			StandardModuleTreeModifier::addSubmodule(module_ref, submodule);

		return module_ref;
	}

	/*********************
	 * ScriptModuleTreeBuilder
	 *********************/

	base::Box<ScriptModuleTreeBuilder> ScriptModuleTreeBuilder::create() {
		return base::makeBox<ScriptModuleTreeBuilder>(ScriptModuleTreeBuilder());
	}

	void ScriptModuleTreeBuilder::setScriptFile(const fs::File& file) {
		assertNotFinalized();
		CORE_ASSERT(!m_script_file.has_value(), "Script file already set");
		m_script_file = file;
	}

	void ScriptModuleTreeBuilder::setParent(base::Ref<ModuleTree> parent) {
		CORE_ASSERT(
			parent->getKind() == ModuleKind::Module, "Parent of a script must be a standard module"
		);
		setParentCommon(parent);
	}

	base::Ref<ModuleTree> ScriptModuleTreeBuilder::finalize() {
		Ref<ModuleTree> module_ref = finalizeCommon(makeBox<ScriptModuleTreeNode>());

		module_ref->as<ScriptModuleTreeNode>()->m_script_file = m_script_file;

		if (m_parent.has_value()) ScriptModuleTreeModifier::setParent(module_ref, m_parent.value());

		return module_ref;
	}

	/*********************
	 * SyntheticReplChainModuleTreeBuilder
	 *********************/

	base::Box<SyntheticReplChainModuleTreeBuilder> SyntheticReplChainModuleTreeBuilder::create() {
		return base::makeBox<SyntheticReplChainModuleTreeBuilder>(
			SyntheticReplChainModuleTreeBuilder()
		);
	}

	void SyntheticReplChainModuleTreeBuilder::setSyntheticSourceFile(const fs::File& file) {
		assertNotFinalized();
		CORE_ASSERT(!m_synthetic_source_file_path.has_value(), "Synthetic source file already set");
		m_synthetic_source_file_path = file;
	}

	void SyntheticReplChainModuleTreeBuilder::setParent(base::Ref<ModuleTree> parent) {
		CORE_ASSERT(
			parent->getKind() == ModuleKind::ReplChain or parent->getKind() == ModuleKind::Script,
			"Parent of a REPL chain module must be a REPL chain module or a script module"
		);
		setParentCommon(parent);
	}

	void SyntheticReplChainModuleTreeBuilder::setParent(ModuleID parent) {
		setParent(GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
			parent
		));
	}

	base::Ref<ModuleTree> SyntheticReplChainModuleTreeBuilder::finalize() {
		Ref<ModuleTree> module_ref = finalizeCommon(makeBox<SyntheticReplChainModuleTreeNode>());

		if (m_parent.has_value())
			SyntheticReplChainModuleTreeModifier::setParent(module_ref, m_parent.value());

		if (m_synthetic_source_file_path.has_value())
			module_ref->as<SyntheticReplChainModuleTreeNode>()->m_synthetic_source_file
				= SourceFile::create(m_synthetic_source_file_path.value(), module_ref);

		return module_ref;
	}
}
