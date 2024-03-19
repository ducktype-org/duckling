/**
 * @file module_tree.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <string>
#include <utility>
#include "base/maps.hpp"
#include "filesystem/file.hpp"
#include "filesystem/fs_parser.hpp"

namespace compiler::frontend {
	constexpr std::string RIFT_SOURCE_FILE      = ".rift";
	constexpr std::string RIFT_MODULE_FILE      = ".rmf";
	constexpr std::string RIFT_MAIN_SOURCE_FILE = "mod.rift";

	class ModuleTree {
	public:
		static std::shared_ptr<ModuleTree> create(auto root) {
			return ModuleTree::create(fs::FsTree::create(root));
		}

		static std::shared_ptr<ModuleTree> create(std::shared_ptr<fs::FsTree> root) {
			auto ptr = std::shared_ptr<ModuleTree>(new ModuleTree());
			ModuleTree::buildModuleTree(ptr, std::move(root));
			return ptr;
		}

		[[nodiscard]]
		base::Optional<const compiler::frontend::ModuleTree&> getParentModule() const;

		[[nodiscard]]
		bool isEmpty() const;

		[[nodiscard]]
		const fs::FilePath& getMainSourceFile() const;

		[[nodiscard]]
		const auto& getSourceFiles() const {
			return m_source_files;
		}

		[[nodiscard]]
		const auto& getSubmodules() const {
			return m_submodules;
		}

		[[nodiscard]]
		const auto& getOtherFiles() const {
			return m_other_files;
		}

		[[nodiscard]]
		std::string getName() const;

		void prettyPrint(u32 indentation = 0) const;


	private:
		ModuleTree() = default;

		static void buildModuleTree(
			const std::shared_ptr<ModuleTree>& moduleRoot, std::shared_ptr<fs::FsTree> treeRoot
		);

		static void
			handleNewFile(const std::shared_ptr<ModuleTree>& module_root, const fs::FilePath& file);

		std::shared_ptr<ModuleTree> m_parent;
		std::shared_ptr<fs::FsTree> m_fs_tree;

		// Has to be unique_ptr, because fs::FilePath does not have a default constructor.
		base::unique_ptr<fs::FilePath>                          m_main_source_file;
		std::vector<fs::FilePath>                               m_source_files;
		base::HashMap<std::string, std::shared_ptr<ModuleTree>> m_submodules;
		base::HashMap<std::string, std::vector<fs::FilePath>>   m_other_files;
	};
}
