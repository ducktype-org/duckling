#pragma once

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <pst_parser/pst.hpp>

#include <base/stable_hashmap.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>

#include <filesystem/file.hpp>

#include <expected>

namespace compiler::frontend {

	class ModuleTree2;

	/**
	 * @brief Represents a source file in the Duckling compiler.
	 */
	class SourceFile final {
		fs::File                   path;
		base::StrID                lang_file_name;
		FileID                     id;
		base::Optional<pst::PST<>> parse_tree;

		static base::StableHashMap<FileID, SourceFile> file_map;

		SourceFile(fs::File path);

		void update();

		friend class ModuleTree2;
	public:

		static Ref<SourceFile> create(fs::File path);

		static Ref<SourceFile> getSourceFile(FileID id);

		static Ref<SourceFile> getSourceFile(const fs::File& file);

		[[nodiscard]] fs::File getPath() const { return path; }

		[[nodiscard]] base::StrID getLangFileName() const {
			return lang_file_name;
		}

		[[nodiscard]] FileID getID() const { return id; }
		/**
		 * @brief Lazily parses the source file and returns PST
		 * @return CRef<pst::PST>
		 */
		CRef<pst::PST<>> getPST();

		/**
		 * @brief Returns cached content for this SourceFile
		 * @note Content is cached during SourceFile construction
		 * @return Cached base::SharedView for this SourceFile
		 */
		[[nodiscard]] base::SharedView getCachedContent() const;

		/**
		 * @brief Returns an unstable perfect hash for this SourceFile.
		 * @details
		 *   The hash is returns the hash of FileID.
		 */
		u64 queryUnstablePerfectHash();
	};
}
