/**
 * @file fs_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#include "fs_parser.hpp"

#include <utility>
#include <iostream>

namespace {}

// The default regexes.
std::regex compiler::frontend::FsTree::default_reject_directory_regex
	= std::regex("(\\$.*|\\.git)");
std::regex compiler::frontend::FsTree::default_reject_file_regex = std::regex("\\$.*");

compiler::frontend::FsTree::FsTree(
	const std::filesystem::path& root,
	std::regex                   reject_file_regex,
	std::regex                   reject_directory_regex
):
	  FsTree(fs::FilePath(root), std::move(reject_file_regex), std::move(reject_directory_regex)) {}

compiler::frontend::FsTree::FsTree(
	fs::FilePath root, std::regex reject_file_regex, std::regex reject_directory_regex
):
	  m_root(std::move(root)),
	  m_reject_file_regex(std::move(reject_file_regex)),
	  m_reject_directory_regex(std::move(reject_directory_regex)) {}

auto compiler::frontend::FsTree::getParentTree() const -> base::Optional<const FsTree&> {
	if (m_parent == nullptr) return {};
	return *m_parent;
}

const fs::FilePath& compiler::frontend::FsTree::getRoot() const { return m_root; }

void compiler::frontend::FsTree::addParent(std::shared_ptr<FsTree> new_parent) {
	m_parent = std::move(new_parent);
}
