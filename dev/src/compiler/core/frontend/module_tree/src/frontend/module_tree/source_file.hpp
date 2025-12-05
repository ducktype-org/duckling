#pragma once


#include <frontend/module_tree/access.hpp>
#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <base/collections/optional.hpp>
#include <base/misc/shared_view.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>
#include <hashing/component_hash.hpp>

namespace compiler::frontend {

	class ModuleTreeModifier;
	class ModuleTree;

	/**
	 * @brief Represents a source file in the Duckling compiler.
	 */
	class SourceFile final {
		fs::File                   file;
		base::StrID                lang_file_name;
		ModuleID                   linked_module;
		base::Optional<pst::PST<>> parse_tree;
		// this is a self pointer, it is necessary to get the FileID from the const SourceFile
		base::Optional<FileID> file_id;
		mutable base::Optional<hashing::ComponentHash>
			component_hash;  //< Logical path hash for this file (module path + file name)

		/**
		 * @brief Constructs a SourceFile and assigns a new FileID.
		 * @param file The file system file.
		 * @param linked_module The module this file belongs to.
		 * @note The file content is cached on construction.
		 */
		SourceFile(fs::File file, ModuleID linked_module);

		/**
		 * @brief Reloads the file content and resets the parse tree.
		 * @note This should only be called from the Language Server (LS) context.
		 *       It is not intended for general use.
		 *       The file content is reloaded from disk and the parse tree is cleared.
		 */
		void update();

		/**
		 * Invalidate the component hash for this source file.
		 */
		void invalidateComponentHash();

		friend class ModuleTreeModifier;
		friend class ModuleTree;
		friend struct ImplementationOf_QueryFilePST;

		bool operator==(const SourceFile& other) const {
			CORE_ASSERT(
				file != other.file || linked_module != other.linked_module
					|| lang_file_name == other.lang_file_name,
				"The same source files should have the same language file name"
			);
			return file == other.file && linked_module == other.linked_module;
		}

	public:
		/**
		 * @brief Creates a new SourceFile or returns an existing one for the given file file.
		 * @param file The file system file.
		 * @param linked_module The module this file belongs to.
		 * @return Reference to the created or existing SourceFile.
		 * @note If a SourceFile for the given file already exists, and the content matches,
		 *       the new SourceFile is returned. If the content differs, an assertion fails.
		 *       A new FileID is always assigned for a new SourceFile.
		 *       The file content is always hashed and cached.
		 *       Each fileID has a unique UnstableHash even if it is pointing to the same fs::File
		 */
		static Ref<SourceFile> create(fs::File file, ModuleID linked_module);

		/**
		 * @brief Retrieves all SourceFile instances associated with the given File.
		 * @param file the fs::File
		 * @return Vector of references to SourceFile instances for the given file.
		 *         If no SourceFiles exist for the file, an empty vector is returned.
		 */
		static std::vector<base::Ref<SourceFile>> getSourceFilesfromFile(const fs::File& file);

		/**
		 * @brief Returns the FileID associated with this SourceFile.
		 */
		[[nodiscard]] FileID getFileID() const { return file_id.value(); }

		/**
		 * @brief Returns the file system file associated with this SourceFile.
		 */
		[[nodiscard]] fs::File getFile() const { return file; }

		/**
		 * @brief Returns the module this SourceFile is linked to.
		 */
		[[nodiscard]] ModuleAccessLocked getModule() const {
			return ModuleAccessLocked(linked_module);
		}

		/**
		 * @brief Returns the language-level file name (stem).
		 */
		[[nodiscard]] base::StrID getLangFileName() const { return lang_file_name; }

		/**
		 * @brief Lazily parses the source file and returns the parse tree (PST).
		 * @return CRef<pst::PST>
		 * @note The parse tree is cached after the first parse.
		 */
		CRef<pst::PST<>> getPST();

		/**
		 * @brief Returns cached content for this SourceFile.
		 * it caches the content when it wasn't previously cached.
		 * @note Content is cached during SourceFile construction.
		 * @return Cached base::SharedView for this SourceFile.
		 * @throws Panics if the content is not found in the cache.
		 */
		[[nodiscard]] base::SharedView getCachedContent();

		[[nodiscard]] const hashing::ComponentHash& getComponentHash() const;

		SourceFile(const SourceFile&)            = delete;
		SourceFile& operator=(const SourceFile&) = delete;
		SourceFile(SourceFile&&) noexcept        = default;
	};
}
