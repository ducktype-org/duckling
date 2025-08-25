#include "source_file.hpp"

#include <base/stable_container.hpp>
#include <base/exceptions.hpp>

#include <filesystem/file.hpp>

namespace {
	// Content cache for each file path (used for deduplication and fast access)
	using ContentMap = base::HashMap<std::filesystem::path, base::SharedView>;
	ContentMap to_content;

	/**
	 * StableVector that stores all SourceFile instances.
	 */
	base::StableVector<compiler::frontend::SourceFile> files;
}

namespace compiler::frontend {

	SourceFile::SourceFile(fs::File file, ModuleID linked_module):
		  file(std::move(file)),
		  linked_module(linked_module) {
		lang_file_name = base::StrID(this->file.getFilePath().stem().c_str());
		// Add or replace file content in cache
		auto abs_path = this->file.getFilePath().absolute().getPath();
	}

	Ref<SourceFile> SourceFile::create(fs::File file, ModuleID linked_module) {
		auto abs_path = file.getFilePath().absolute().getPath();

		if (to_content.contains(abs_path)) {
			CORE_ASSERT(
				to_content.at(abs_path).view() == file.getContent().view(),
				base::strConcat(
					"SourceFile with path '",
					abs_path.string(),
					"' already exists with different content. "
					"Delete the existing SourceFile first or call update handler from the ModuleModifier."
				)
			);
		} else {
			to_content.put(abs_path, file.getContent());
		}

		files.pushBack(SourceFile(std::move(file), linked_module));
		return files.last();
	}

	void SourceFile::update() {
		auto abs_path = this->file.getFilePath().absolute().getPath();
		// Update content in cache
		to_content.put(abs_path, this->file.getContent());
		lang_file_name = base::StrID(this->file.getFilePath().stem().c_str());
		// reset the parse tree
		parse_tree.reset();
	}

	void SourceFile::erase() {
		auto abs_path = this->file.getFilePath().absolute().getPath();
		// Remove content from cache
		to_content.erase(abs_path);
	}

	CRef<pst::PST<>> SourceFile::getPST() {
		if (parse_tree) {
			return &parse_tree.value();
		} else {
			parse_tree.emplace(pst::PST(file));
			return &parse_tree.value();
		}
	}

	base::SharedView SourceFile::getCachedContent() const {
		auto abs_path = this->file.getFilePath().absolute().getPath();
		auto it       = to_content.find(abs_path);
		if (it != to_content.end()) return it->second;
		// This should not happen since content is cached in constructor
		CORE_PANIC("SourceFile content not found in cache for: " + abs_path.string());
	}
}
