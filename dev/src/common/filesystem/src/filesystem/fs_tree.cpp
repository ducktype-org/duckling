/**
 * @file fs_parser.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */
#include "fs_tree.hpp"

using std::regex;
using namespace std::filesystem;
using namespace fs;

// These regexes catch anything, that starts with '.' or '$'.
// These values are used by compiler::frontend::ModuleTree.
regex FsTree::default_reject_directory_regex = regex(R"((\$.*|\..*))");
regex FsTree::default_reject_file_regex      = regex(R"((\$.*|\..*))");

FsTree::FsTree(
	const std::filesystem::path& root, regex reject_file_regex, regex reject_directory_regex
):
	  FsTree(fs::File(root), std::move(reject_file_regex), std::move(reject_directory_regex)) {}

FsTree::FsTree(fs::File root, regex reject_file_regex, regex reject_directory_regex):
	  m_reject_file_regex(std::move(reject_file_regex)),
	  m_reject_directory_regex(std::move(reject_directory_regex)),
	  m_root(std::move(root)) {}

auto FsTree::getParentTree() const -> base::Optional<base::CRef<FsTree>> {
	if (m_parent.has_value()) {
		CORE_ASSERT(not m_parent.value().expired(), "Parent of FsTree is expired");
		return &*m_parent.value().lock();
	}
	return {};
}

const fs::File& FsTree::getRoot() const { return m_root; }

void FsTree::addParent(const std::shared_ptr<FsTree>& new_parent) { m_parent = new_parent; }

void FsTree::recursiveCreate(const std::shared_ptr<FsTree>& root) {
	for (const auto& path: root->getRoot().listFilePaths()) {
		// If we don't check for this, then we might get some weird cycles.
		if (path.isSymlink()) continue;

		if (path.isDirectory()) {
			// If a file is a directory, then instantiate a new FsTree with path as its root.
			if (!root->isDirectoryNameValid(path.name())) continue;
			auto child = create(path, root->m_reject_file_regex, root->m_reject_directory_regex);
			child->addParent(root);
			auto child_name = child->getRoot().name();
			if (root->m_dirs.contains(child_name)) {
				throw base::LogicError(base::strConcat(
					"Not unique directory name: ",
					child_name,
					" at: ",
					path.nativePath(),
					" root: ",
					root->getRoot().name()
				));
			}
			root->m_dirs.put(child_name, child);
		} else {
			// If a file is not a directory, then add it to FsTree's files storage.
			if (!root->isFileNameValid(path.name())) continue;
			auto file_name = path.name();
			if (root->m_files.contains(file_name))
				throw base::LogicError(base::strConcat(
					"Not unique file name: ",
					file_name,
					", at: ",
					path.nativePath(),
					", root: ",
					root->getRoot().name()
				));
			root->m_files.put(file_name, path);
		}
	}
}

bool FsTree::isFileNameValid(const std::string& filename) const {
	std::smatch _match;
	return !std::regex_match(filename, _match, m_reject_file_regex);
}

bool FsTree::isDirectoryNameValid(const std::string& dirname) const {
	std::smatch _match;
	return !std::regex_match(dirname, _match, m_reject_directory_regex);
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

const base::HashMap<std::string, fs::File>& FsTree::getFiles() const { return m_files; }
