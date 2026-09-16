#include <lsp_interface/server_session.hpp>

#include <algorithm>

namespace duck_ls {

	namespace {
		/**
		 * @brief Whether `path` is `root` itself or lies below it, matched component by component.
		 */
		bool isUnderRoot(const fs::FilePath& path, const fs::FilePath& root) {
			auto path_it  = path.getPath().begin();
			auto path_end = path.getPath().end();

			for (const auto& root_part: root.getPath()) {
				if (path_it == path_end) return false;
				if (*path_it != root_part) return false;
				++path_it;
			}

			return true;
		}
	}

	void ServerSession::setFiles(base::Box<IFilesManagement> files_management) {
		files = std::move(files_management);
	}

	IFilesManagement& ServerSession::filesManagement() {
		return *files.expect("The files management of the session was never installed");
	}

	fs::FilePath ServerSession::cachePath(const fs::FilePath& source) {
		// A test builds its "disk" in the singleton VFS, so the source can already be virtual;
		// rebinding it keeps the same spelling, which the module walk relies on.
		if (source.isVirtual()) return source.withVfs(base::Ref<fs::VFS>(&cache_vfs));
		return source.toVirtualPath(base::Ref<fs::VFS>(&cache_vfs));
	}

	void ServerSession::addWorkspaceRoot(const fs::FilePath& root) {
		if (std::ranges::find(workspace_roots, root) != workspace_roots.end()) return;
		workspace_roots.push_back(root);
	}

	base::Optional<fs::FilePath> ServerSession::workspaceRootFor(const fs::FilePath& path) const {
		auto it = std::ranges::find_if(workspace_roots, [&](const fs::FilePath& root) {
			return isUnderRoot(path, root);
		});
		if (it == workspace_roots.end()) return {};
		return *it;
	}

}
