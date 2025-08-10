#pragma once

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <pst_parser/pst.hpp>

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>
#include <base/stable_hashmap.hpp>

#include <filesystem/file.hpp>

#include <expected>

namespace compiler::frontend {

	class ModuleTreeModifier;
	class ModuleTree;

	/**
	 * @brief Represents a source file in the Duckling compiler.
	 */
	class SourceFile final {
		fs::File                   path;
		base::StrID                lang_file_name;
		FileID                     id;
		base::CRef<ModuleTree>     linked_module;
		base::Optional<pst::PST<>> parse_tree;

		static base::StableHashMap<FileID, SourceFile> file_map;

		/**
		 * @brief Constructs a SourceFile and assigns a new FileID.
		 * @param path The file system file.
		 * @param linked_module The module this file belongs to.
		 * @note The file content is cached on construction.
		 */
		SourceFile(fs::File path, base::CRef<ModuleTree> linked_module);

		/**
		 * @brief Reloads the file content and resets the parse tree.
		 * @note This should only be called from the Language Server (LS) context.
		 *       It is not intended for general use.
		 *       The file content is reloaded from disk and the parse tree is cleared.
		 */
		void update();

		friend class ModuleTreeModifier;
		friend class ModuleTree;

	public:
		/**
		 * @brief Creates a new SourceFile or returns an existing one for the given file path.
		 * @param path The file system file.
		 * @param linked_module The module this file belongs to.
		 * @return Reference to the created or existing SourceFile.
		 * @note If a SourceFile for the given path already exists, and the content matches,
		 *       the new SourceFile is returned. If the content differs, an assertion fails.
		 *       A new FileID is always assigned for a new SourceFile.
		 *       The file content is always hashed and cached.
		 *       Each fileID has a unique UnstableHash even if it is pointing to the same fs::File
		 */
		static Ref<SourceFile> create(fs::File path, base::CRef<ModuleTree> linked_module);

		/**
		 * @brief Returns a reference to the SourceFile with the given FileID.
		 * @param id The FileID.
		 * @return Reference to the SourceFile.
		 * @throws Assertion if the FileID does not exist.
		 */
		static Ref<SourceFile> getSourceFile(FileID id);

		/**
		 * @brief Returns a reference to the SourceFile for the given file path.
		 * @param file The file system file.
		 * @return Reference to the SourceFile.
		 * @throws Assertion if the file does not exist in the map.
		 */
		static Ref<SourceFile> getSourceFile(const fs::File& file);

		/**
		 * @brief Returns the file system file associated with this SourceFile.
		 */
		[[nodiscard]] fs::File getFile() const { return path; }

		/**
		 * @brief Returns the module this SourceFile is linked to.
		 */
		[[nodiscard]] base::CRef<ModuleTree> getModule() const { return linked_module; }

		/**
		 * @brief Returns the language-level file name (stem).
		 */
		[[nodiscard]] base::StrID getLangFileName() const { return lang_file_name; }

		/**
		 * @brief Returns the FileID of this SourceFile.
		 */
		[[nodiscard]] FileID getID() const { return id; }

		/**
		 * @brief Lazily parses the source file and returns the parse tree (PST).
		 * @return CRef<pst::PST>
		 * @note The parse tree is cached after the first parse.
		 */
		CRef<pst::PST<>> getPST();

		/**
		 * @brief Returns cached content for this SourceFile.
		 * @note Content is cached during SourceFile construction.
		 * @return Cached base::SharedView for this SourceFile.
		 * @throws Assertion if the content is not found in the cache.
		 */
		[[nodiscard]] base::SharedView getCachedContent() const;

		/**
		 * @brief Returns an unstable perfect hash for this SourceFile.
		 * @details
		 *   The hash is the hash of FileID.
		 */
		u64 queryUnstablePerfectHash();
	};
}
