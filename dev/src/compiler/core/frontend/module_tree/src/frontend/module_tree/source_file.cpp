#include "source_file.hpp"

#include "concurrent/base/collections/hash_map.hpp"

#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/collections/stable_hashmap.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <filesystem/file.hpp>

namespace {
	// Content cache for each file path (used for deduplication and fast access)
	using ContentMap = concurrent::ConHashMap<std::filesystem::path, base::SharedView>;
	ContentMap to_content;

	/**
	 * StableHashMap that stores all SourceFile instances.
	 */
	concurrent::ConHashMap<usize, compiler::frontend::SourceFile> files;
	std::atomic<usize>                                            next_storage_key = 0;

	/*
	 * Map that stores all SourceFile instances by their file path.
	 * it is used for function getSourceFilesFromFile()
	 */
	concurrent::ConHashMap<
		std::filesystem::path,
		std::vector<base::Ref<compiler::frontend::SourceFile>>>
		files_map;

}

namespace compiler::frontend {

	SourceFile::SourceFile(fs::File file, ModuleID linked_module):
		  state_lock(base::makeBox<concurrent::AtomicFlagSpinlock>()),
		  file(std::move(file)),
		  linked_module(linked_module) {
		lang_file_name = base::StrID(this->file.getFilePath().stem().c_str());
		// Add or replace file content in cache
		auto abs_path = this->file.getFilePath().absolute().getPath();
	}

	Ref<SourceFile> SourceFile::create(fs::File file, ModuleID linked_module) {
		auto abs_path = file.getFilePath().absolute().getPath();

		const auto storage_key = next_storage_key.fetch_add(1);
		auto       inserted    = files.put(storage_key, SourceFile(std::move(file), linked_module));
		Ref<SourceFile> created_ref(&inserted->value);
		created_ref->storage_handle = storage_key;
		created_ref->file_id        = FileID(created_ref);

		files_map.maybePutAndUpdate(
			abs_path,
			std::vector<base::Ref<SourceFile>>{ created_ref },
			[&](Ref<std::vector<base::Ref<SourceFile>>> entries) { entries->push_back(created_ref); }
		);

		return created_ref;
	}

	std::vector<base::Ref<SourceFile>> SourceFile::getSourceFilesFromFile(const fs::File& file) {
		auto abs_path = file.getFilePath().absolute().getPath();
		return files_map.atMaybeCopy(abs_path).copyValueOr(std::vector<base::Ref<SourceFile>>{});
	}

	void SourceFile::update() {
		auto abs_path = this->file.getFilePath().absolute().getPath();
		// Update content in cache
		to_content.erase(abs_path);
		lang_file_name = base::StrID(this->file.getFilePath().stem().c_str());
		// reset the parse tree
		parse_tree.reset();
	}

	const hashing::ComponentHash& SourceFile::getComponentHash() const {
		if (!component_hash.has_value()) {
			auto m_path_component_hash = ModuleTree::getPathComponentHash(linked_module);
			component_hash = hashing::ComponentHash(m_path_component_hash, lang_file_name);
		}
		return component_hash.value();
	}

	CRef<pst::PST<>> SourceFile::getPST() {
		std::lock_guard lock(*state_lock);

		// If component hash changed, reset parse tree
		if (parse_tree && component_hash.has_value()) {
			return &parse_tree.value();
		} else {
			// @TODO: #1879 Program chosen as default type
			parse_tree.emplace(pst::PST(file, pst::PSTType::Program, getComponentHash()));
			return &parse_tree.value();
		}
	}

	base::SharedView SourceFile::getCachedContentIllegalAccess() {
		auto abs_path = this->file.getFilePath().absolute().getPath();

		to_content.maybePut(abs_path, file.getContent());

		IF_BUILD_TYPE_DEV({
			auto content = to_content.atMaybeCopy(abs_path);
			if (content.has_value()) {
				CORE_ASSERT(
					to_content.at(abs_path)->view() == file.getContent().view(),
					base::strConcat(
						"SourceFile with path '",
						abs_path.string(),
						"' already exists with different content. "
						"Delete the existing SourceFile first or call "
						"update handler from the ModuleModifier."
					)
				);
			}
		});

		// This should not happen since content is cached in constructor
		CORE_PANIC("SourceFile content not found in cache for: " + abs_path.string());
	}

	void SourceFile::invalidateComponentHash() { component_hash.reset(); }

	void SourceFile::removeSourceFileFromStorage(Ref<SourceFile> source_file) {
		auto abs_path = source_file->file.getFilePath().absolute().getPath();

		files_map.maybePutAndUpdate(
			abs_path, {}, [&](Ref<std::vector<base::Ref<SourceFile>>> entries) {
				std::erase(*entries, source_file);
			}
		);


		// if (files_map.contains(abs_path)) {
		// 	auto&      entries = files_map.at(abs_path);
		// 	const auto removal = std::ranges::remove_if(
		// 		entries.begin(),
		// 		entries.end(),
		// 		[source_file](const base::Ref<SourceFile>& candidate) {
		// 			return candidate == source_file;
		// 		}
		// 	);
		// 	entries.erase(removal.begin(), removal.end());

		// 	if (entries.empty()) {
		// 		files_map.erase(abs_path);
		// 		to_content.erase(abs_path);
		// 	}
		// }

		CORE_ASSERT(
			source_file->storage_handle.has_value(),
			"Attempted to remove SourceFile without storage handle"
		);
		const auto storage_key = source_file->storage_handle.value();
		const bool erased      = files.erase(storage_key);
		CORE_ASSERT(erased, "Failed to remove SourceFile from storage");
	}

	void SourceFile::checkDanglingReference(const base::Ref<SourceFile>& candidate) {
		IF_BUILD_TYPE_DEV({
			// If we are not using module modifier, skip the check
			if (!use_module_modifier_remove) return;
			const auto* candidate_ptr = candidate.get();
			bool        is_tracked    = false;
			for (const auto& entry: files) {
				if (&entry.value == candidate_ptr) {
					is_tracked = true;
					break;
				}
			}
			if (!is_tracked) CORE_PANIC("dangling reference used after removing SourceFile");
		});
	}
}
