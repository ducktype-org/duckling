/**
 * @file fs_parser.hpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#pragma once
#include <string_view>
#include <regex>
#include <utility>
#include "filesystem/file.hpp"

namespace compiler::frontend {
	class FsTree {
	public:
		static std::regex default_reject_file_regex;
		static std::regex default_reject_directory_regex;

		static std::shared_ptr<FsTree> create(
			const auto& root,
			std::regex  reject_file_regex      = default_reject_file_regex,
			std::regex  reject_directory_regex = default_reject_directory_regex
		) {
			auto ptr = std::shared_ptr<FsTree>(
				new FsTree(root, std::move(reject_file_regex), std::move(reject_directory_regex))
			);

			for (const auto& path:
			     std::filesystem::directory_iterator(ptr->getRoot().getStdPath())) {
				if (path.is_directory()) {
					auto child
						= create(path, ptr->m_reject_file_regex, ptr->m_reject_directory_regex);
					child->addParent(ptr);
					auto child_name = child->getRoot().name();
					if (ptr->m_dirs.contains(child_name))
						throw std::logic_error(base::strConcat("Not unique name: ", child_name));
					ptr->m_dirs.put(child_name, child);
				} else {
					auto file_path = fs::FilePath(path);
					auto file_name = file_path.name();
					if (ptr->m_files.contains(file_name))
						throw std::logic_error(base::strConcat("Not unique file name: ", file_name)
						);
					ptr->m_files.put(file_name, file_path);
				}
			}

			return ptr;
		}

		[[nodiscard]]
		auto iterDirs() const {
			return m_dirs;
		}

		[[nodiscard]]
		auto iterFiles() const {
			return m_files;
		}

		[[nodiscard]]
		base::Optional<const FsTree&> getParentTree() const;

		[[nodiscard]]
		const fs::FilePath& getRoot() const;

	private:
		friend std::shared_ptr<FsTree>;

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

		fs::FilePath m_root;

		std::regex m_reject_file_regex;
		std::regex m_reject_directory_regex;

		std::shared_ptr<FsTree>                             m_parent;
		base::HashMap<std::string, fs::FilePath>            m_files;
		base::HashMap<std::string, std::shared_ptr<FsTree>> m_dirs;

		void addParent(std::shared_ptr<FsTree> new_parent);
	};
}
