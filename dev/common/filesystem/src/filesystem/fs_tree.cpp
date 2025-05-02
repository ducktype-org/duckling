/**
 * @file fs_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#include "fs_tree.hpp"

using Regex = regex::Regex;
using namespace std::filesystem;
using namespace fs;

// These regexes catch anything, that starts with '.' or '$'.
// These values are used by compiler::frontend::ModuleTree.
Regex FsTree::default_reject_directory_regex = Regex(R"((\$.*|\..*))");
Regex FsTree::default_reject_file_regex      = Regex(R"((\$.*|\..*))");

FsTree::FsTree(
	const std::filesystem::path& root, Regex reject_file_regex, Regex reject_directory_regex
):
	  FsTree(fs::FilePath(root), std::move(reject_file_regex), std::move(reject_directory_regex)) {}

FsTree::FsTree(fs::FilePath root, Regex reject_file_regex,Regex reject_directory_regex):
	  m_reject_file_regex(std::move(reject_file_regex)),
	  m_reject_directory_regex(std::move(reject_directory_regex)),
	  m_root(std::move(root)) {}

auto FsTree::getParentTree() const -> base::Optional<const FsTree&> {
	if (m_parent.has_value()) {
		CORE_ASSERT(not m_parent.value().expired(), "Parent of FsTree is expired");
		return *m_parent.value().lock();
	}
	return {};
}

const fs::FilePath& FsTree::getRoot() const { return m_root; }

void FsTree::addParent(const std::shared_ptr<FsTree>& new_parent) { m_parent = new_parent; }

void FsTree::recursiveCreate(const std::shared_ptr<FsTree>& root) {
	for (const auto& path: root->getRoot().directoryIterator()) {
		// If we don't check for this, then we might get some weird cycles.
		if (path.is_symlink()) continue;

		auto fs_path = FilePath(path);
		if (path.is_directory()) {
			// If a file is a directory, then instantiate a new FsTree with path as its root.
			if (!root->isDirectoryNameValid(fs_path.name())) continue;
			auto child = create(fs_path, root->m_reject_file_regex, root->m_reject_directory_regex);
			child->addParent(root);
			auto child_name = child->getRoot().name();
			if (root->m_dirs.contains(child_name)) {
				throw base::LogicError(base::strConcat(
					"Not unique directory name: ",
					child_name,
					" at: ",
					fs_path.absolutePath(),
					" root: ",
					root->getRoot().name()
				));
			}
			root->m_dirs.put(child_name, child);
		} else {
			// If a file is not a directory, then add it to FsTree's files storage.
			if (!root->isFileNameValid(fs_path.name())) continue;
			auto file_name = fs_path.name();
			if (root->m_files.contains(file_name))
				throw base::LogicError(base::strConcat(
					"Not unique file name: ",
					file_name,
					", at: ",
					fs_path.absolutePath(),
					", root: ",
					root->getRoot().name()
				));
			root->m_files.put(file_name, fs_path);
		}
	}
}

bool FsTree::isFileNameValid(const std::string& filename) const {
	return !m_reject_file_regex.match(filename);
}

bool FsTree::isDirectoryNameValid(const std::string& dirname) const {
	return !m_reject_directory_regex.match(dirname);
}

bool FsTree::isEmpty() const { return m_files.empty() && m_dirs.empty(); }

std::string FsTree::prettyPrint(const u32 indentation) const {
	std::string indent;
	for (u32 i = 0; i < indentation; i++) indent += (i % 3 == 0 ? "│" : " ");

	std::stringstream output;
	output << indent << getRoot().name() << "/\n";

	for (const auto& subdir: getDirs()) output << subdir.second->prettyPrint(indentation + 3);

	for (const auto& file_iter: getFiles())
		output << indent << "├─ " << file_iter.second.name() << '\n';

	return output.str();
}

const base::HashMap<std::string, std::shared_ptr<FsTree>>& FsTree::getDirs() const {
	return m_dirs;
}

const base::HashMap<std::string, fs::FilePath>& FsTree::getFiles() const { return m_files; }
