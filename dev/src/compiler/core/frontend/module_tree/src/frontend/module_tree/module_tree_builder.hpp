#pragma once

#include "module_id.hpp"
#include "module_tree.hpp"

#include <base/collections/maps.hpp>
#include <base/collections/optional.hpp>
#include <base/pointers/box.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>

#include <regex>
#include <vector>

namespace compiler::frontend {
	/**
	 * ModuleTreeBuilder - Builder class for constructing ModuleTree instances.
	 *
	 * Allows step-by-step construction of module trees with assertions that check the correctness
	 * of module creation.
	 */
	class ModuleTreeBuilder final {
	public:
		/**
		 * Creates a new builder instance.
		 * @return Boxed ModuleTreeBuilder.
		 */
		static base::Box<ModuleTreeBuilder> create();

		/**
		 * Creates a new builder instance with a random package ID.
		 * This is used for testing purposes.
		 * @return Boxed ModuleTreeBuilder.
		 */
		static base::Box<ModuleTreeBuilder> createWithRandomPackageID();

		void setKind(ModuleKind kind);

		/**
		 * Factory method to create ModuleTree from filesystem tree.
		 * @param root Pre-constructed fs::File with a module structure.
		 * @param file_resolver Lambda, given a regular file on disk, may open a different file
		 * (useful in the LS). Never called for directories.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 * @return A valid pointer with the root.
		 */
		static Ref<ModuleTree> create(
			const fs::File&     root,
			base::StrID         package_id,
			const FileResolver& file_resolver = identityFileResolver(),
			const std::regex&   file_reject   = DEFAULT_REJECT_FILE_REGEX,
			const std::regex&   dir_reject    = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * Factory method to create ModuleTree from filesystem tree with random package ID.
		 * This is used for testing purposes.
		 * @param root Pre-constructed fs::File with a module structure.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 * @return A valid pointer with the root.
		 */
		static Ref<ModuleTree> createWithRandomPackageID(
			const fs::File&   root,
			const std::regex& file_reject = DEFAULT_REJECT_FILE_REGEX,
			const std::regex& dir_reject  = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * Sets the main source file for the module.
		 * @param file The main source file.
		 */
		void setMainSourceFile(const fs::File& file);

		/**
		 * Adds a submodule to the module being built.
		 * @param submodule The submodule to add.
		 */
		void addSubmodule(base::Ref<ModuleTree> submodule);

		/**
		 * Adds an other file to the module being built.
		 * @param file The file to add.
		 */
		void addOtherFile(const fs::File& file);

		/**
		 * Sets the name of the module.
		 * @param name The name to set.
		 */
		void setName(base::StrID name);

		/**
		 * Sets the package ID for the module tree.
		 * The package ID must be set for every module tree
		 * @param package_id The package ID to set.
		 */
		void setPackageID(base::StrID package_id);

		/**
		 * Sets the parent module.
		 * @param parent The parent module.
		 */
		void setParent(base::Ref<ModuleTree> parent);

		/**
		 * Builds the module tree from a single file (single-file module).
		 * @param file The file to build from.
		 */
		void buildFromSingleFile(const fs::File& file, base::StrID package_id);

		/**
		 * Checks if the builder is finalized.
		 * @return True if finalized, false otherwise.
		 */
		[[nodiscard]]
		bool isFinalized() const;

		/**
		 * Finalizes the construction and returns the built ModuleTree.
		 * After calling this, the builder becomes invalid.
		 * @return The constructed ModuleTree.
		 */
		base::Ref<ModuleTree> finalize();

	private:
		/**
		 * Constructs a ModuleTreeBuilder.
		 */
		ModuleTreeBuilder();

		/**
		 * Builds the module tree from a directory structure.
		 * This will recursively traverse the directory and build the module tree.
		 * @param directory The root directory to build the module tree from.
		 * @param file_resolver Lambda, given a regular file on disk, may open a different file
		 * instead.
		 * @param file_reject Regex for rejecting files.
		 * @param dir_reject Regex for rejecting directories.
		 */
		void buildFromDirectory(
			const fs::File&     directory,
			base::StrID         package_id,
			const FileResolver& file_resolver = identityFileResolver(),
			const std::regex&   file_reject   = DEFAULT_REJECT_FILE_REGEX,
			const std::regex&   dir_reject    = DEFAULT_REJECT_DIRECTORY_REGEX
		);

		/**
		 * Handles a new file found during directory traversal.
		 * This is a helper function used when creating module tree from fs::File.
		 * @param file The file to handle.
		 */
		void handleNewFile(const fs::File& file);

		base::Optional<ModuleKind>                        kind;
		base::Optional<base::Ref<ModuleTree>>             m_parent;
		base::Optional<fs::File>                          m_main_source_file_path;
		base::StrID                                       m_package_id;
		base::HashMap<base::StrID, base::Ref<ModuleTree>> m_submodules;
		base::HashMap<base::StrID, std::vector<fs::File>> m_other_files;

		base::StrID m_name;
		bool        m_finalized;
	};
}
