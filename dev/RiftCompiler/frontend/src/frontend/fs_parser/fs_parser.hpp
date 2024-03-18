/**
 * @file fs_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include <string_view>
#include <regex>
#include <utility>
#include <future>
#include "filesystem/file.hpp"

namespace compiler::frontend {
	class FsTree {
	public:
		static std::regex default_reject_file_regex;
		static std::regex default_reject_directory_regex;

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

		[[nodiscard]]
		const auto& getDirs() const {
			return m_dirs;
		}

		[[nodiscard]]
		const auto& getFiles() const {
			return m_files;
		}

		[[nodiscard]]
		base::Optional<const FsTree&> getParentTree() const;

		[[nodiscard]]
		const fs::FilePath& getRoot() const;

		[[nodiscard]]
		bool isEmpty() const;

	private:
		explicit FsTree(
			fs::FilePath root,
			std::regex   reject_file_regex      = default_reject_file_regex,
			std::regex   reject_directory_regex = default_reject_directory_regex
		);

		explicit FsTree(
			const std::filesystem::path& root,
			std::regex                   reject_file_regex      = default_reject_file_regex,
			std::regex                   reject_directory_regex = default_reject_directory_regex
		);

		static void recursiveCreate(const std::shared_ptr<FsTree>& root);

		std::regex m_reject_file_regex;
		std::regex m_reject_directory_regex;

		fs::FilePath                                        m_root;
		std::shared_ptr<FsTree>                             m_parent;
		base::HashMap<std::string, fs::FilePath>            m_files;
		base::HashMap<std::string, std::shared_ptr<FsTree>> m_dirs;

		[[nodiscard]]
		bool isFileNameValid(const std::string& filename) const;

		[[nodiscard]]
		bool isDirectoryNameValid(const std::string& dirname) const;

		void addParent(std::shared_ptr<FsTree> new_parent);
	};
}
