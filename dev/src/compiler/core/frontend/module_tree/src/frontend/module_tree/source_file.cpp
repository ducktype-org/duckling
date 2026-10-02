#include "source_file.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <frontend/module_tree/file_id.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/collections/stable_hashmap.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <filesystem/file.hpp>

namespace {


	/**
	 * @brief A value pair storing the information about a file.
	 */
	struct PathState final {
		/// List of all SourceFile instances associated with this path.
		std::vector<base::Ref<compiler::frontend::SourceFile>> instances;
		/// Cached view of the file's content (used for deduplication and fast access).
		base::Optional<base::SharedView> content;
	};

	// Map that stores all SourceFile instanced and the content cache by their file path.
	concurrent::ConHashMap<fs::FilePath, PathState> path_registry;

	/**
	 * Concurrent Stable HashMap that stores all SourceFile instances.
	 */
	concurrent::ConHashMap<usize, compiler::frontend::SourceFile> files;
	std::atomic<usize>                                            next_storage_key = 0;
}

namespace compiler::frontend {

	SourceFile::SourceFile(fs::File file, ModuleID linked_module):
		  state_lock(base::makeBox<std::recursive_mutex>()),
		  file(std::move(file)),
		  linked_module(linked_module) {
		lang_file_name = base::StrID(this->file.getFilePath().stem().c_str());
	}

	Ref<SourceFile> SourceFile::create(fs::File file, ModuleID linked_module) {
		auto abs_path = file.getFilePath().absolute();

		const auto storage_key = next_storage_key.fetch_add(1);
		auto       inserted    = files.put(storage_key, SourceFile(std::move(file), linked_module));
		Ref<SourceFile> created_ref(&inserted->value);
		created_ref->storage_handle = storage_key;
		created_ref->file_id        = FileID(created_ref);

		path_registry.maybePutAndUpdate(
			abs_path,
			PathState{ .instances = {}, .content = std::nullopt },
			[&](Ref<PathState> state) { state->instances.push_back(created_ref); }
		);
		return created_ref;
	}

	std::vector<base::Ref<SourceFile>> SourceFile::getSourceFilesFromPath(const fs::FilePath& path) {
		auto abs_path = path.absolute();

		auto state = path_registry.atMaybeCopy(abs_path);
		if (state.has_value()) return state->instances;
		return {};
	}

	void SourceFile::update() {
		std::scoped_lock lock(*state_lock);

		auto abs_path = this->file.getFilePath().absolute();

		// Update content in cache
		path_registry.maybePutAndUpdate(abs_path, PathState{}, [&](Ref<PathState> state) {
			state->content.reset();
		});

		lang_file_name = base::StrID(this->file.getFilePath().stem().c_str());
		// Reset the parse tree.
		parse_tree.reset();
	}

	const hashing::ComponentHash& SourceFile::getComponentHash() const {
		std::scoped_lock lock(*state_lock);
		if (!component_hash.has_value()) {
			auto m_path_component_hash = ModuleTree::getPathComponentHash(linked_module);
			component_hash = hashing::ComponentHash(m_path_component_hash, lang_file_name);
		}
		return component_hash.value();
	}

	CRef<pst::PST<>> SourceFile::getPST() {
		std::scoped_lock lock(*state_lock);

		// If component hash changed, reset parse tree
		if (parse_tree && component_hash.has_value()) {
			return &parse_tree.value();
		} else {
			// @TODO: #1879 Program chosen as default type for non_REPL
			auto pst_type = getModuleRef(linked_module)->isReplModule() ? pst::PSTType::Script
			                                                            : pst::PSTType::Program;

			auto parsed_pst = pst::PST(file, pst_type, getComponentHash());

			// Illegal access is fine here because we are outside of any query and the PST is only
			// being created.
			if (parsed_pst.getRootElement().illegalAccess().has_value()) {
				// @TODO: #2397 we could change it, such that root element is never null.
				// Set additional root data only if the root element is not null:
				parsed_pst.setAdditionalRootData(pst::AdditionalRootData{
					.pst_parent = pst::AdditionalRootData::ModuleParent{ this->linked_module, },
				});
			}
			parse_tree.emplace(std::move(parsed_pst));

			return &parse_tree.value();
		}
	}

	base::SharedView SourceFile::getCachedContentIllegalAccess() {
		auto abs_path = this->file.getFilePath().absolute();

		path_registry.maybePutAndUpdate(
			abs_path,
			PathState{ .instances = {}, .content = file.getContent() },
			[&](Ref<PathState> state) {
				if (!state->content.has_value()) {
					state->content = file.getContent();
				} else {
					CORE_ASSERT(
						state->content->view() == file.getContent().view(),
						base::strConcat(
							"SourceFile with path '",
							abs_path.string(),
							"' already exists with different content. "
							"Delete the existing SourceFile first or call "
							"update handler from the ModuleModifier."
						)
					);
				}
			}
		);

		return *path_registry.at(abs_path)->content;
	}

	void SourceFile::invalidateComponentHash() {
		std::scoped_lock lock(*state_lock);
		component_hash.reset();
	}

	void SourceFile::removeSourceFileFromStorage(Ref<SourceFile> source_file) {
		auto abs_path = source_file->file.getFilePath().absolute();

		// Remove the `Path -> (SourceFiles, SharedView)` if the value vector is empty.
		path_registry.eraseIf(abs_path, [&](Ref<PathState> state) {
			std::erase(state->instances, source_file);
			return state->instances.empty();
		});

		CORE_ASSERT(
			source_file->storage_handle.has_value(),
			"Attempted to remove SourceFile without storage handle"
		);
		const auto storage_key = source_file->storage_handle.value();
		const bool erased      = files.erase(storage_key);
		CORE_ASSERT(erased, "Failed to remove SourceFile from storage");
	}

	void SourceFile::checkDanglingReference([[maybe_unused]] const base::Ref<SourceFile>& candidate
	) {
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
