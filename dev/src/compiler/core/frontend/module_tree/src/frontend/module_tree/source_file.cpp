#include "source_file.hpp"

#include <base/exceptions.hpp>

#include <filesystem>

namespace compiler::frontend {

	SourceFile::ContentMap SourceFile::to_content;

	SourceFile::SourceFile(fs::File path, ModuleID module_id):
		  path(std::move(path)),
		  id(FileID::next()),
		  linked_module(module_id) {
		lang_file_name = base::StrID(this->path.stem().c_str());
		// Add or replace file content in cache
		auto abs_path = this->path.absolutePath();
		if (to_content.contains(abs_path))
			to_content[abs_path] = path.getContent();
		else
			to_content.put(abs_path, path.getContent());
	}

	CRef<pst::PST<>> SourceFile::getPST() {
		if (parse_tree) {
			return &parse_tree.value();
		} else {
			parse_tree.emplace(pst::PST(path));
			return &parse_tree.value();
		}
	}

	fs::FileContent SourceFile::getCachedContent(const fs::File& path) {
		auto it = to_content.find(path.absolutePath());
		if (it != to_content.end()) return it->second;
		return {};
	}

	u64 SourceFile::queryUnstablePerfectHash() {
		static u64                             next_hash = 0;
		static base::HashMap<std::string, u64> hash_map;
		std::string                            abs_path = path.absolutePath();
		if (hash_map.contains(abs_path)) return hash_map.at(abs_path);
		u64 hash = next_hash++;
		hash_map.put(abs_path, hash);
		return hash;
	}
}
