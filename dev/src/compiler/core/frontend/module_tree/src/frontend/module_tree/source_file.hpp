// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once


#include <frontend/module_tree/access.hpp>
#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <frontend/pst_parser/parsed_pst.hpp>
#include <frontend/pst_parser/pst_id.hpp>

#include <base/collections/optional.hpp>
#include <base/misc/shared_view.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>
#include <hashing/component_hash.hpp>

#include <mutex>

namespace compiler::frontend {

	class ModuleTreeModifier;
	class ModuleTree;
	struct GetFileID_Functor;

	/**
	 * @brief Represents a source file in the Duckling compiler.
	 */
	class SourceFile final {
		/**
		 * @brief Synchronizes access to state of SourceFile. Since multiple workers may try to
		 * parse the same SourceFile simultaneously.
		 * @note Recursive mutex is required because getPST() can call getComponentHash(), and both
		 * functions lock this mutex.
		 * @note: It's possible that there are two SourceFiles pointing to the same physical file in
		 * the file system. In this case, two threads may parse the same physical file at once, but
		 * since this operation is read-only, it's thread-safe.
		 */
		mutable base::Box<std::recursive_mutex> state_lock;
		fs::File                                file;
		base::StrID                             lang_file_name;
		MBox<pst::ParsedPST<>>                  parse_tree;
		Ref<ModuleTree>                         linked_module;
		base::Optional<usize> storage_handle;  //< Key to support removal from static storage
		// this is a self pointer, it is necessary to get the FileID from the const SourceFile
		base::Optional<Ref<SourceFile>> self;
		mutable base::Optional<hashing::ComponentHash>
			component_hash;  //< Logical path hash for this file (module path + file name)
		//< Any functions that actually modifies it like invalidateComponentHash should not be
		// marked const

		/**
		 * @brief Constructs a SourceFile and assigns a new FileID.
		 * @param file The file system file.
		 * @param linked_module The module this file belongs to.
		 * @note The file content is cached on construction.
		 */
		SourceFile(fs::File file, Ref<ModuleTree> linked_module);

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
		friend struct GetFileID_Functor;
		friend struct FileID;

		/**
		 * Ensures a SourceFile reference still points to a tracked instance.
		 * This function will work only in dev build if use_module_modifier_remove flag is enabled.
		 */
		static void checkDanglingReference(const base::Ref<SourceFile>& candidate);

		/**
		 * @brief Returns the file registered under the given component hash.
		 * A file is registered every time its component hash is computed; entries are never
		 * removed, so a hash of a removed file that was not recreated resolves to a dangling
		 * reference.
		 */
		static Ref<SourceFile> getRegisteredFile(const base::Bit256& hash);

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
		 *       The file content is always hashed and cached.
		 *       The FileID is derived from the module path and the file name, so two SourceFiles
		 *       with the same name in the same module share it.
		 *       The module may still be under construction; its hash is not computed here.
		 */
		static Ref<SourceFile> create(fs::File file, Ref<ModuleTree> linked_module);

		/**
		 * @brief Retrieves all SourceFile instances registered under the given path.
		 *
		 * The registry is keyed by path, so the question can be asked about a file that is no
		 * longer there: a path whose file has been deleted is exactly the case where a leftover
		 * registration matters.
		 *
		 * @param path the path the files were registered under
		 * @return Vector of references to SourceFile instances for the given path.
		 *         If no SourceFiles exist for the path, an empty vector is returned.
		 */
		static std::vector<base::Ref<SourceFile>> getSourceFilesFromPath(const fs::FilePath& path);

		/**
		 * @brief Returns the FileID associated with this SourceFile.
		 */
		[[nodiscard]] FileID getFileID() const { return { self.value() }; }

		/**
		 * @brief Returns the file system file associated with this SourceFile.
		 * You can use it only outside the query.
		 */
		[[nodiscard]] fs::File getFileIllegalAccess() const { return file; }

		/**
		 * @brief Returns the module this SourceFile is linked to.
		 */
		[[nodiscard]] ModuleAccessLocked getModule() const;

		/**
		 * @brief Returns the language-level file name (stem).
		 */
		[[nodiscard]] base::StrID getLangFileName() const { return lang_file_name; }

		/**
		 * @brief Lazily parses the source file and returns the parse tree (PST).
		 * @return CRef<pst::PST>
		 * @note The parse tree is cached after the first parse.
		 */
		CRef<pst::ParsedPST<>> getPST();

		/**
		 * @brief Returns cached content for this SourceFile.
		 * it caches the content when it wasn't previously cached.
		 * @note Content is cached during SourceFile construction.
		 * @return Cached base::SharedView for this SourceFile.
		 * @throws Panics if the content is not found in the cache.
		 */
		[[nodiscard]] base::SharedView getCachedContentIllegalAccess();

		[[nodiscard]] const hashing::ComponentHash& getComponentHash() const;

		/**
		 * @brief Remove SourceFile.
		 * @note This will invalidate all references!
		 * In principle it should only be used in ModuleTreeModifier in pair with query invalidations.
		 */
		static void removeSourceFileFromStorage(Ref<SourceFile> source_file);

		SourceFile(const SourceFile&)            = delete;
		SourceFile& operator=(const SourceFile&) = delete;
		SourceFile(SourceFile&&) noexcept        = default;
	};

}
