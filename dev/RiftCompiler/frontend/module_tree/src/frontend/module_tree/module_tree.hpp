/**
 * @file module_tree.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once

#include <base/strongly_typed_id.hpp>
#include <base/maps.hpp>
#include <base/perfect_hash.hpp>
#include <base/ints.hpp>
#include <filesystem/file.hpp>
#include <filesystem/fs_tree.hpp>
#include <pst_parser/pst.hpp>

#include <string>

// @TODO: change std::string here to StrId

namespace compiler::frontend {
	/**
	 * @brief Structure holding FileID within SourceFile
	 */
	struct FileId {
		[[nodiscard]]
		u64 asInt() const {
			return id;
		}

		static FileId nextID();
		bool          operator==(const FileId&) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return asInt();
		}

	private:
		u64 id;
		FileId() = default;
	};

	/**
	 * @brief Structure holding SourceFile within Module Tree
	 */
	struct SourceFile {
		fs::FilePath               path;
		std::string                rift_file_name;  // or StrID?
		FileId                     id;
		base::Optional<pst::PST<>> parse_tree;

		SourceFile(fs::FilePath);

		/**
		 * @brief Lazily parses the source file and returns PST
		 * @return const pst::PST&
		 */
		const pst::PST<>& getPST();
	};

	STRONG_TYPEDEF_ID(ModuleId);

	// @TODO: move to STRONG_TYPEDEF_ID?
	inline base::HashT customPerfectHash(ModuleId id) { return id.asInt(); }

	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a source file of the module.
	 */
	constexpr std::string_view RIFT_SOURCE_FILE = ".rift";

	/**
	 * If a file's extension is equal to this constant, then it is assumed
	 * it is a single file module.
	 */
	constexpr std::string_view RIFT_MODULE_FILE = ".rmf";

	/**
	 * `ModuleTree` contains source files, modules and other
	 * files from a directory specified as root. If a module does not contain
	 * a module file, then it is omitted.
	 *
	 * It is a recursive data structure.
	 *
	 * In order to construct ModuleTree use a factory: `ModuleTree::create(root)`.
	 */
	class ModuleTree {
		/**
		 * If a file matches this regex, then it is omitted.
		 * The value is set by fs::FsTree::default_reject_file_regex.
		 */
		constexpr static std::regex& default_reject_file_regex
			= fs::FsTree::default_reject_file_regex;
		/**
		 * If a directory matches this regex, then it is omitted.
		 * The value is set by fs::FsTree::default_reject_directory_regex.
		 */
		constexpr static std::regex& default_reject_directory_regex
			= fs::FsTree::default_reject_directory_regex;

	public:
		/**
		 * The main factory to construct `ModuleTree`s.
		 * @param root (std::string/fs::FilePath/std::filesystem::path...) - anything,
		 * that can be used to construct fs::FsTree.
		 * @param reject_file_regex A regex to check against whether
		 * a file should be omitted.
		 * @param reject_directory_regex A regex to check against whether
		 * a directory should be omitted.
		 * @return A valid pointer with the root.
		 */
		static std::shared_ptr<ModuleTree> create(
			auto       root,
			std::regex reject_file_regex      = default_reject_file_regex,
			std::regex reject_directory_regex = default_reject_directory_regex
		) {
			return ModuleTree::create(
				fs::FsTree::create(root, reject_file_regex, reject_directory_regex)
			);
		}

		/**
		 * An alternative factory, does not construct a new `fs::FsTree`, but uses
		 * the one that is passed.
		 * @param root Pre-constructed std::shared_ptr<fs::FsTree> with a module structure.
		 * @return A valid pointer with the root.
		 */
		static std::shared_ptr<ModuleTree> create(std::shared_ptr<fs::FsTree> root);

		/**
		 * Accessor to module's parent module. A module might not have a parent module.
		 * @return If a module has parent module, then a reference to it is passed
		 * inside the base::Optional.
		 */
		[[nodiscard]]
		base::Optional<const ModuleTree&> getParentModule() const;

		/**
		 * Checks if a module contains `RIFT_MAIN_SOURCE_FILE`.
		 * @return True if pointer is valid, false otherwise.
		 */
		[[nodiscard]]
		bool hasMainSourceFile() const;

		/**
		 * Accesses the main `RIFT_MAIN_SOURCE_FILE` - main source file of the module.
		 * If a pointer to file is invalid, then throws an std::logic_error exception.
		 * @return A reference to the `RIFT_MAIN_SOURCE_FILE`.
		 */
		[[nodiscard]]
		const SourceFile& getMainSourceFile() const;

		/**
		 * Accesses the source files of the module.
		 * @return A std::vector<SourceFile> with `RIFT_SOURCE_FILE` files to iterate over.
		 */
		[[nodiscard]]
		const std::vector<SourceFile>& getSourceFiles() const;

		/**
		 * Accesses the submodules located in this submodule. Submodules are indexed by their name.
		 * @return base::HashMap that maps a name of the submodule to the pointer to the submodule.
		 */
		[[nodiscard]]
		const base::HashMap<std::string, std::shared_ptr<ModuleTree>>& getSubmodules() const;

		/**
		 * Accesses all the other files that are located inside the module.
		 * @return A base::HashMap that maps a file extension to a vector
		 * with files with this extension.
		 */
		[[nodiscard]]
		const base::HashMap<std::string, std::vector<fs::FilePath>>& getOtherFiles() const;

		/**
		 * Parses the name of the module.
		 * @return std::string with the name. `A.rmf -> A`, `/.../module/ -> module`.
		 */
		[[nodiscard]]
		std::string getName() const;

		/**
		 * Creates a nice, human-readable representation of this module tree.
		 * @param indentation For regular printing, leave 0.
		 * @return std::string with the representation.
		 */
		std::string prettyPrint(u32 indentation = 0) const;

		/**
		 * Fetches the id of the module.
		 * @return compiler::frontend::ModuleId.
		 */
		[[nodiscard]]
		ModuleId getId() const;

	private:
		ModuleTree();

		/**
		 * Recursively builds the ModuleTree inplace on the module_tree.
		 * @param module_root A pointer to the ModuleTree.
		 * @param tree_root A FsTree pointer, that will be used to get information
		 * about the folder structure.
		 */
		static void buildModuleTree(
			const std::shared_ptr<ModuleTree>& module_root, std::shared_ptr<fs::FsTree> tree_root
		);

		/**
		 * Adds the file to the module - inserts it
		 * to m_main_source_file/m_source_files/m_submodules according to its type.
		 * @param module_root A pointer to ModuleTree, where the file should be inserted.
		 */
		static void handleNewFile(
			const std::shared_ptr<ModuleTree>& module_root, const fs::FilePath& filepath
		);

		/**
		 * Id of the current root Module.
		 */
		ModuleId id;
		/**
		 * A pointer to the module's parent. Might be nullptr.
		 */
		std::weak_ptr<ModuleTree> m_parent;
		/**
		 * A pointer to the file system tree, that this structure is mapping.
		 */
		std::shared_ptr<fs::FsTree> m_fs_tree;

		/**
		 * A link to the main source file.
		 *
		 * Has to be a container (like base::Optional), because fs::FilePath does
		 * not have a default constructor.
		 */
		base::Optional<SourceFile> m_main_source_file;
		/**
		 * All the source files in the module. Does not contain files of other submodules.
		 */
		std::vector<SourceFile> m_source_files;
		/**
		 * Other direct submodules. Maps module's name to a pointer to it.
		 */
		base::HashMap<std::string, std::shared_ptr<ModuleTree>> m_submodules;
		/**
		 * All other files inside this module. Indexed by their extension.
		 */
		base::HashMap<std::string, std::vector<fs::FilePath>> m_other_files;

		// @TODO: change strings here to string-ids
	};
}

// std::hash functor for ModuleID and FileID:
namespace std {
	template<>
	struct hash<compiler::frontend::ModuleId> {
		usize operator()(const compiler::frontend::ModuleId& k) const { return k.asInt(); }
	};

	template<>
	struct hash<compiler::frontend::FileId> {
		usize operator()(const compiler::frontend::FileId& k) const { return k.asInt(); }
	};
}
