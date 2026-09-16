#pragma once

#include <lsp_interface/files_management.hpp>
#include <lsp_interface/server_session.hpp>

#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>

namespace duck_ls {

	/**
	 * @brief Derives a package ID that stays the same across reloads of the same directory.
	 *
	 * The package ID is the root of every component hash, so a random one would make every
	 * reload a full recompile.
	 */
	base::StrID packageIdForRoot(const fs::FilePath& package_root);

	/**
	 * @brief The compiler-backed implementation: keeps the module tree in step with what the
	 * editor holds open.
	 */
	class CompilerFilesManagement final: public IFilesManagement {
	public:
		explicit CompilerFilesManagement(base::Ref<ServerSession> session);

		void addWorkspace(const fs::FilePath& root) override;
		void openDocument(const fs::FilePath& path, std::string_view text) override;
		void updateDocument(const fs::FilePath& path, std::string_view text) override;
		void closeDocument(const fs::FilePath& path) override;
		void fileCreatedOrDeletedOnDisk(const fs::FilePath& path) override;

	private:
		/**
		 * @brief Finds the package root owning `path`, bounded by the workspace root.
		 */
		base::Optional<fs::FilePath> findPackageRoot(const fs::FilePath& path) const;

		/**
		 * @brief Walks `package_root` into the module tree and registers it as a package.
		 */
		void loadPackage(const fs::FilePath& package_root);

		/**
		 * @brief Tears the package owning `path` down and walks it again from disk.
		 */
		void reloadPackageOwning(const fs::FilePath& path);

		/**
		 * @brief Swaps the file backing `path`'s module for `replacement`, invalidating only the
		 * queries that depended on the old one.
		 */
		bool swapMainSourceFile(const fs::FilePath& path, const fs::File& replacement);

		base::Ref<ServerSession> session;
	};

}
