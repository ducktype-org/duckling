#pragma once

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <pst_parser/pst.hpp>

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>

#include <filesystem/file.hpp>

#include <expected>

namespace compiler::frontend {

	/**
	 * @brief Structure representing Duckling  source file.
	 * It is currently assumed that source files exist only within Module Trees.
	 */
	struct SourceFile final {
		fs::File                   path;
		base::StrID                lang_file_name;
		FileID                     id;
		base::Optional<pst::PST<>> parse_tree;

		/**
		 * @brief Module the file belongs to
		 * @note: in the future there might be module-less files
		 */
		ModuleID linked_module;

		SourceFile(fs::File, ModuleID linked_module);

		/**
		 * @brief Lazily parses the source file and returns PST
		 * @return CRef<pst::PST>
		 */
		CRef<pst::PST<>> getPST();

		/**
		 * @brief Returns cached content for this SourceFile
		 * @note Content is cached during SourceFile construction
		 * @return Cached FileContent for this SourceFile
		 */
		[[nodiscard]] fs::FileContent getCachedContent() const;

		/**
		 * @brief Returns an unstable perfect hash for this SourceFile.
		 * @details
		 *   The hash is currently computed based on the file's path.
		 */
		u64 queryUnstablePerfectHash();
	};
}
