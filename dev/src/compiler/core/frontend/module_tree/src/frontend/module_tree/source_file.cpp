#include "source_file.hpp"

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/collections/optional.hpp>
#include <base/collections/stable_container.hpp>
#include <base/except/exceptions.hpp>

#include <filesystem/file.hpp>

namespace {

	// Content cache for each file path (used for deduplication and fast access)
	using ContentMap = base::HashMap<std::filesystem::path, base::SharedView>;
	ContentMap to_content;

	/**
	 * StableVector that stores all SourceFile instances.
	 */
	base::StableVector<compiler::frontend::SourceFile> files;

	/*
	 * Map that stores all SourceFile instances by their file path.
	 * it is used for function getSourceFilesfromFile()
	 */
	base::HashMap<std::filesystem::path, std::vector<base::Ref<compiler::frontend::SourceFile>>>
		files_map;
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

		if (!files_map.contains(abs_path))
			files_map.put(abs_path, std::vector<base::Ref<SourceFile>>());
		files.pushBack(SourceFile(std::move(file), linked_module));
		files_map[abs_path].emplace_back(files.last());
		files.last()->file_id = FileID(files.last());
		return files.last();
	}

	std::vector<base::Ref<SourceFile>> SourceFile::getSourceFilesfromFile(const fs::File& file) {
		auto abs_path = file.getFilePath().absolute().getPath();
		if (files_map.contains(abs_path)) return files_map[abs_path];
		return {};
	}

	void SourceFile::update() {
		auto abs_path = this->file.getFilePath().absolute().getPath();
		// Update content in cache
		to_content.erase(abs_path);
		lang_file_name = base::StrID(this->file.getFilePath().stem().c_str());
		// reset the parse tree
		parse_tree.reset();
	}

	const hashing::ComponentHash& SourceFile::getComponentHash() {
		if (!component_hash.has_value()) {
			auto m_component_hash = ModuleTree::getComponentHash(linked_module);
			component_hash        = hashing::ComponentHash(m_component_hash, lang_file_name);
		}
		return component_hash.value();
	}

	CRef<pst::PST<>> SourceFile::getPST() {
		// If component hash changed, reset parse tree
		if (parse_tree && component_hash.has_value()) {
			return &parse_tree.value();
		} else {
			parse_tree.emplace(pst::PST(file, getComponentHash()));
			return &parse_tree.value();
		}
	}

	base::SharedView SourceFile::getCachedContent() {
		auto abs_path = this->file.getFilePath().absolute().getPath();
		if (to_content.contains(abs_path)) {
			CORE_ASSERT(
				to_content[abs_path].view() == file.getContent().view(),
				base::strConcat(
					"SourceFile with path '",
					abs_path.string(),
					"' already exists with different content. "
					"Delete the existing SourceFile first or call "
					"update handler from the ModuleModifier."
				)
			);
		} else {
			to_content.put(abs_path, file.getContent());
		}
		if_opt_some(to_content.atMaybe(abs_path), content) return *content;
		// This should not happen since content is cached in constructor
		CORE_PANIC("SourceFile content not found in cache for: " + abs_path.string());
	}

	void SourceFile::invalidateComponentHash() { component_hash.reset(); }
}
