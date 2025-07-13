#include "source_file.hpp"

#include <filesystem/file.hpp>

#include <base/exceptions.hpp>

namespace {
	using ContentMap
		= base::HashMap<std::filesystem::path, base::SharedView, std::hash<std::filesystem::path>>;
	ContentMap to_content;
}

namespace compiler::frontend {
	SourceFile::SourceFile(fs::File path, ModuleID module_id):
		  path(std::move(path)),
		  id(FileID::next()),
		  linked_module(module_id) {
		lang_file_name = base::StrID(this->path.stem().c_str());
		// Add or replace file content in cache
		auto abs_path = this->path.nativePath();
		if (!to_content.contains(abs_path)) to_content.put(abs_path, this->path.getContent());
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
		auto abs_path = this->path.nativePath();
		auto it       = to_content.find(abs_path);
		if (it != to_content.end()) return it->second;
		// This should not happen since content is cached in constructor
		CORE_PANIC("SourceFile content not found in cache for: " + abs_path);
	}

	u64 SourceFile::queryUnstablePerfectHash() {
		static u64                             next_hash = 0;
		static base::HashMap<std::string, u64> hash_map;
		std::string                            abs_path = path.nativePath();
		if (hash_map.contains(abs_path)) return hash_map.at(abs_path);
		u64 hash = next_hash++;
		hash_map.put(abs_path, hash);
		return hash;
	}
}
