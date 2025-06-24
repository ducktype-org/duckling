#pragma once

#include <filesystem/file.hpp>
#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <pst_parser/pst.hpp>

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>

#include <expected>

namespace compiler::frontend {

	/**
	 * @brief Structure holding SourceFile within Module Tree
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

		// --- Content cache and hash logic ---
		using ContentMap
			= base::HashMap<std::filesystem::path, fs::FileContent, std::hash<std::filesystem::path>>;

		static ContentMap to_content;

		static fs::FileContent getCachedContent(const fs::File& path);

		u64 queryUnstablePerfectHash();
	};
}
