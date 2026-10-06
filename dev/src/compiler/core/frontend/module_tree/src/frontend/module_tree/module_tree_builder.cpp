// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "module_tree_builder.hpp"

#include "module_tree_modifier.hpp"
#include "source_file.hpp"

#include <base/except/exceptions.hpp>

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

	Ref<ModuleTree> ModuleTreeBuilder::create(
		const fs::File&     root,
		base::StrID         package_id,
		const FileResolver& file_resolver,
		const std::regex&   file_reject,
		const std::regex&   dir_reject
	) {
		base::Box<ModuleTreeBuilder> builder = ModuleTreeBuilder::create();

		if (root.isDirectory())
			builder->buildFromDirectory(root, package_id, file_resolver, file_reject, dir_reject);
		else
			builder->buildFromSingleFile(root, package_id);

		return builder->finalize();
	}

	Ref<ModuleTree> ModuleTreeBuilder::createWithRandomPackageID(
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

	void ModuleTreeBuilder::buildFromDirectory(
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
				base::Box<ModuleTreeBuilder> submodule_builder = ModuleTreeBuilder::create();
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

	void ModuleTreeBuilder::buildFromSingleFile(const fs::File& file, base::StrID package_id) {
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

	void ModuleTreeBuilder::handleNewFile(const fs::File& file) {
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
				base::Box<ModuleTreeBuilder> submodule_builder = ModuleTreeBuilder::create();
				submodule_builder->buildFromSingleFile(file, this->m_package_id);
				auto submodule = submodule_builder->finalize();
				CORE_ASSERT(submodule->getName() == stem_id, "Submodule name does not match");
				addSubmodule(base::Ref<ModuleTree>(submodule));
			}
		} else {
			// Other file
			addOtherFile(file);
		}
	}

	/*********************
	 * ModuleTreeBuilder Implementation
	 *********************/

	ModuleTreeBuilder::ModuleTreeBuilder(): m_finalized(false) {}

	base::Box<ModuleTreeBuilder> ModuleTreeBuilder::create() {
		return base::makeBox<ModuleTreeBuilder>(ModuleTreeBuilder());
	}

	base::Box<ModuleTreeBuilder> ModuleTreeBuilder::createWithRandomPackageID() {
		base::Box<ModuleTreeBuilder> builder = ModuleTreeBuilder::create();
		builder->setPackageID(base::StrID(base::generateRandomString(32)));
		return builder;
	}

	void ModuleTreeBuilder::setMainSourceFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(!m_main_source_file_path.has_value(), "Main source file already set");
		m_main_source_file_path = file;
	}

	void ModuleTreeBuilder::addSubmodule(base::Ref<ModuleTree> submodule) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(
			!m_submodules.contains(submodule->getName()),
			"Submodule with the same name already added"
		);
		m_submodules.put(submodule->getName(), submodule);
	}

	void ModuleTreeBuilder::addOtherFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		if (!m_other_files.contains(ext_id)) m_other_files.put(ext_id, std::vector<fs::File>());
		m_other_files.at(ext_id).push_back(file);
	}

	void ModuleTreeBuilder::setName(base::StrID name) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(m_name.isBad(), "Module name is already set");
		m_name = name;
	}

	void ModuleTreeBuilder::setParent(base::Ref<ModuleTree> parent) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		m_parent = parent;
	}

	void ModuleTreeBuilder::setReplModule(const ReplData& repl_data) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		m_repl_data = repl_data;
	}

	void ModuleTreeBuilder::setPackageID(base::StrID package_id) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(m_package_id.isBad(), "Package ID is already set");
		m_package_id = package_id;
	}

	bool ModuleTreeBuilder::isFinalized() const { return m_finalized; }

	base::Ref<ModuleTree> ModuleTreeBuilder::finalize() {
		CORE_ASSERT(!m_finalized, "Builder already finalized");

		m_finalized = true;

		// Create new ModuleTree instance
		Ref<ModuleTree> module_ref = ModuleTree::addModuleToStorage();

		module_ref->m_self = module_ref;

		// Set ID and name
		module_ref->m_name        = m_name;
		module_ref->m_other_files = std::move(m_other_files);

		CORE_ASSERT(m_package_id.isGood(), "Package ID must be set for every module tree!");
		module_ref->m_package_id = m_package_id;

		// Set REPL-specific attributes
		module_ref->m_repl_data = m_repl_data;

		if (m_parent.has_value()) ModuleTreeModifier::setParent(module_ref, m_parent);

		// Create SourceFiles from stored paths
		if (m_main_source_file_path.has_value()) {
			module_ref->m_main_source_file
				= SourceFile::create(m_main_source_file_path.value(), ModuleID(module_ref));
		}

		for (const auto& [name, submodule]: m_submodules)
			ModuleTreeModifier::addSubmodule(module_ref, submodule);

		return module_ref;
	}
}
