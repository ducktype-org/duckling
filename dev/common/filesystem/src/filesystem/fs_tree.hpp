/**
 * @file fs_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include <regex>
#include <future>
#include "filesystem/file.hpp"

namespace fs {
	/**
	 * A recursive structure, that holds information about a filesystem.
	 * In order to construct this structure, use FsTree::create factory.
	 */
	class FsTree {
	public:
		/**
		 * If a file matches to this regex, then it is omitted.
		 * The value is set in module_tree.cpp.
		 */
		static std::regex default_reject_file_regex;
		/**
		 * If a directory matches to this regex, then it is omitted.
		 * The value is set in module_tree.cpp.
		 */
		static std::regex default_reject_directory_regex;

		/**
		 * The main factory of FsTree. Constructs a new FsTree.
		 * @param root Any object, that can be used as a path:
		 * std::string/fs::FilePath/std::filesystem::path.
		 * @param file_reject A regex used to reject files.
		 * @param dir_reject A regex used to reject directories.
		 * @return
		 */
		static std::shared_ptr<FsTree> create(
			const auto&       root,
			const std::regex& file_reject = default_reject_file_regex,
			const std::regex& dir_reject  = default_reject_directory_regex
		) {
			// Using `new` to access the private constructors.
			auto ptr = std::shared_ptr<FsTree>(new FsTree(root, file_reject, dir_reject));
			recursiveCreate(ptr);
			return ptr;
		}

		/**
		 * Accesses directories inside this directory.
		 * @return Map name -> FsTree of the directory.
		 */
		[[nodiscard]]
		const base::HashMap<std::string, std::shared_ptr<FsTree>>& getDirs() const;

		/**
		 * Accesses files inside this directory.
		 * @return Map name -> file in the directory.
		 */
		[[nodiscard]]
		const base::HashMap<std::string, FilePath>& getFiles() const;

		/**
		 * Accessor to tree's parent tree. A tree might not have an link to parent tree.
		 * @return If a tree has parent tree, then a reference to it is passed
		 * inside the base::Optional.
		 */
		[[nodiscard]]
		base::Optional<const FsTree&> getParentTree() const;

		/**
		 * Accesses the directory, that was used as a FsTree root.
		 * @return Path to the file.
		 */
		[[nodiscard]]
		const FilePath& getRoot() const;

		/**
		 * Checks if the tree has any files or directories inside.
		 * @return True if has any files or directories, false otherwise.
		 */
		[[nodiscard]]
		bool isEmpty() const;

		/**
		 * Creates a nice, human-readable representation of this filesystem tree.
		 * @param indentation For regular printing, leave 0.
		 * @return std::string with the representation.
		 */
		std::string prettyPrint(u32 indentation = 0) const;

	private:
		explicit FsTree(
			FilePath   root,
			std::regex reject_file_regex      = default_reject_file_regex,
			std::regex reject_directory_regex = default_reject_directory_regex
		);

		explicit FsTree(
			const std::filesystem::path& root,
			std::regex                   reject_file_regex      = default_reject_file_regex,
			std::regex                   reject_directory_regex = default_reject_directory_regex
		);

		/**
		 * Recursively construct the tree inplace of root.
		 * @param root The tree to be used as a base.
		 * @return The unique pointer that was initially passed to the function.
		 */
		static void recursiveCreate(const std::shared_ptr<FsTree>& root);

		/**
		 * An owned by tree version of a regex for rejecting files.
		 */
		std::regex m_reject_file_regex;
		/**
		 * An owned by tree version of a regex for rejecting directories.
		 */
		std::regex m_reject_directory_regex;

		/**
		 * A link to the directory used as a root.
		 */
		FilePath m_root;
		/**
		 * A pointer to the tree's parent tree. Might be nullptr.
		 */
		std::weak_ptr<FsTree> m_parent;
		/**
		 * A map of filenames to the appropriate fs::FilePath from inside this directory.
		 */
		base::HashMap<std::string, FilePath> m_files;
		/**
		 * A map of directory names to the appropriate FsTrees from inside this directory.
		 */
		base::HashMap<std::string, std::shared_ptr<FsTree>> m_dirs;

		/**
		 * Validates a filename with m_reject_file_regex.
		 * @param filename The filename to verify.
		 * @return True if it doesn't match the regex, false otherwise.
		 */
		[[nodiscard]]
		bool isFileNameValid(const std::string& filename) const;

		/**
		 * Validates a directory's name with m_reject_directory_regex.
		 * @param dirname The directory's name to verify.
		 * @return True if it doesn't match the regex, false otherwise.
		 */
		[[nodiscard]]
		bool isDirectoryNameValid(const std::string& dirname) const;

		/**
		 * Inserts a parent to the FsTree.
		 * @param new_parent The parent to be inserted.
		 */
		void addParent(const std::shared_ptr<FsTree>& new_parent);
	};
}
