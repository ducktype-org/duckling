#include "source_file.hpp"
#ifdef DEBUG
#include <unordered_set>
#endif

#include <base/exceptions.hpp>

#include <filesystem/file.hpp>

namespace {
	using ContentMap
		= base::HashMap<std::filesystem::path, base::SharedView>;
	ContentMap to_content;

	base::HashMap<std::filesystem::path, compiler::frontend::FileID> file_id_map;
#ifdef DEBUG
	std::unordered_set<compiler::frontend::FileID> changed_files_ids;
#endif
}

namespace compiler::frontend {
	SourceFile::SourceFile(fs::File path, base::CRef<ModuleTree> linked_module):
		  path(std::move(path)),
		  id(FileID::next()),
		  linked_module(linked_module) {
		lang_file_name = base::StrID(this->path.getFilePath().stem().c_str());
		// Add or replace file content in cache
		auto abs_path = this->path.getFilePath().absolute().getPath();
		to_content.put(abs_path, this->path.getContent());
		file_id_map.put(abs_path, id);
		file_map.put(id, *this);
	}

	Ref<SourceFile> SourceFile::create(fs::File path, base::CRef<ModuleTree> linked_module) {
		auto abs_path = path.getFilePath().absolute().getPath();

		CORE_ASSERT(!to_content.contains(abs_path), base::strConcat(
			"SourceFile with path `", abs_path.string(), "` already exists!"
		));

		return new SourceFile(std::move(path), linked_module);
	}

	Ref<SourceFile> SourceFile::getSourceFile(FileID id) {
#ifdef DEBUG
		CORE_ASSERT(
			!changed_files_ids.contains(id),
			"SourceFile with ID " + std::to_string(id.asInt()) + " has changed since last query. Please use the new FileID."
		);
#endif
		CORE_ASSERT(
			file_map.contains(id),
			"SourceFile with ID " + std::to_string(id.asInt()) + " does not exist!"
		);
		return file_map.atMaybe(id).value();
	}

	Ref<SourceFile> SourceFile::getSourceFile(const fs::File& file) {
		auto abs_path = file.getFilePath().absolute().getPath();
		CORE_ASSERT(file_id_map.contains(abs_path), "SourceFile with path `" + abs_path.string() + "` does not exist!");
		return getSourceFile(file_id_map.at(abs_path));
	}

	void SourceFile::update(bool content_changed) {
		auto abs_path = this->path.getFilePath().absolute().getPath();
		CORE_ASSERT(to_content.contains(abs_path), "SourceFile with path `" + abs_path.string() + "` does not exist in cache!");
			// If content has changed, update the cache
#ifdef DEBUG
			changed_files_ids.insert(id);
#endif
			file_map.erase(id);
			id = FileID::next();

			file_id_map.put(abs_path, id);
			file_map.put(id, *this);

			if(content_changed) {
				// Update content in cache
				to_content.put(abs_path, this->path.getContent());
				lang_file_name = base::StrID(this->path.getFilePath().stem().c_str());
			}

			//clear the parse tree
			//@TODO: we need this if file content is not changed?
			// this question is about PST having something like a cache form mangled names/ module names etc
			parse_tree.reset();
	}

	CRef<pst::PST<>> SourceFile::getPST() {
		if (parse_tree) {
			return &parse_tree.value();
		} else {
			parse_tree.emplace(pst::PST(path));
			return &parse_tree.value();
		}
	}

	base::SharedView SourceFile::getCachedContent() const {
		auto abs_path = this->path.getFilePath().absolute().getPath();
		auto it       = to_content.find(abs_path);
		if (it != to_content.end()) return it->second;
		// This should not happen since content is cached in constructor
		CORE_PANIC("SourceFile content not found in cache for: " + abs_path.string());
	}

	u64 SourceFile::queryUnstablePerfectHash() {
		return id.queryUnstablePerfectHash();
	}
}
