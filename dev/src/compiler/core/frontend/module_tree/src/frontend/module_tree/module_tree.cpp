#include "module_tree.hpp"

#include "access.hpp"
#include "functors.hpp"
#include "module_flags/module_flags.hpp"
#include "module_tree_builder.hpp"
#include "queries.hpp"
#include "source_file.hpp"

#include <concurrent/base/collections/hash_map.hpp>
#include <concurrent/worker/worker_manager.hpp>
#include <frontend/packages/access.hpp>

#include <base/collections/stable_hashmap.hpp>
#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <query_framework/standard_query/query_cache_macros.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <string_id/string_id.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <ranges>
#include <regex>
#include <sstream>
#include <string_view>

namespace {
	/**
	 * StableHashMap that stores all ModuleTree instances.
	 */
	base::StableHashMap<usize, Box<compiler::frontend::ModuleTree>> modules;
	usize                                                      next_module_storage_key = 0;
}

namespace compiler::frontend {

	const hashing::ComponentHash& ModuleTree::getPathComponentHash(ModuleID module_id) {
		Ref<ModuleTree> module = module_id.ref;
		// Update the component hash from root to this module if not valid
		module->updateModuleHashFromRootToThis();
		CORE_ASSERT(
			module->m_path_component_hash.has_value(),
			"Component hash should have value after update!"
		);
		return module->m_path_component_hash.value();
	}

	const hashing::ComponentHash::HashType& ModuleTree::getModuleHash(ModuleID module_id) {
		Ref<ModuleTree> module = module_id.ref;
		module->updateModuleHashFromRootToThis();
		CORE_ASSERT(module->m_hash.has_value(), "Module hash should have value after update!");
		return module->m_hash.value();
	}

	const FileResolver& identityFileResolver() {
		static const FileResolver resolver = [](const fs::File& disk_file) { return disk_file; };
		return resolver;
	}

	ModuleTree::ModuleTree(): m_hash_recompute_mutex(base::makeBox<std::mutex>()) {}

	ModuleTree::~ModuleTree() = default;

	ModuleID ModuleTree::getModuleID() const { return m_id.value(); }

	ModuleKind ModuleTree::getKind() const {
		CORE_ASSERT(kind != ModuleKind::Invalid, "Module kind should never be invalid at this point!");
		return kind;
	}

	base::Optional<ModuleAccessLocked> ModuleTree::getParentModule() const {
		if (m_parent.has_value()) return ModuleAccessLocked(m_parent.value()->getModuleID());
		return {};
	}

	std::vector<base::Ref<SourceFile>> ModuleTree::collectOwnedSourceFiles() const {
		std::vector<base::Ref<SourceFile>> source_files;
		auto                               slot = mainSourceFileSlot();
		if (slot != nullptr && slot->has_value()) source_files.push_back(slot->value());
		return source_files;
	}

	bool ModuleTree::hasMainSourceFile() const {
		auto slot = mainSourceFileSlot();
		return slot != nullptr && slot->has_value();
	}

	FileAccessLocked ModuleTree::getMainSourceFile() const {
		auto slot = mainSourceFileSlot();
		CORE_ASSERT(slot != nullptr && slot->has_value(), "Main source file does not exist!");
		return FileAccessLocked(slot->value()->getFileID());
	}

	SubmodulesAccessLocked ModuleTree::getSubmodules() const {
		std::vector<ModuleAccessLocked> submodules;
		if (auto submodules_map = submodulesSlot(); submodules_map != nullptr) {
			submodules.reserve(submodules_map->size());
			for (const auto& [name, submodule]: *submodules_map)
				submodules.emplace_back(submodule->getModuleID());
		}
		return { getModuleID(), std::move(submodules) };
	}

	packages::PackageAccessLocked ModuleTree::getPackage() const {
		return packages::PackageAccessLocked(m_package_id);
	}

	ModuleChildAccessLocked ModuleTree::getSubmoduleByName(base::StrID name) const {
		base::Optional<ModuleID> child;
		if (auto submodules_map = submodulesSlot(); submodules_map != nullptr)
			if (auto maybe = submodules_map->atMaybe(name); maybe.has_value())
				child = (*maybe.value())->getModuleID();
		return ModuleChildAccessLocked(getModuleID(), name, child);
	}

	const base::HashMap<base::StrID, std::vector<fs::File>>& ModuleTree::getOtherFiles() const {
		static const base::HashMap<base::StrID, std::vector<fs::File>> no_other_files;
		auto                                                           other_files = otherFilesSlot();
		return other_files != nullptr ? *other_files : no_other_files;
	}

	base::StrID ModuleTree::getName() const { return m_name; }

	std::string ModuleTree::humanReadableID(query::Context& ctx) const {
		const auto own_name
			= getName().isBad() ? std::string_view("<unnamed>") : getName().strView();

		auto parent = getParentModule();
		if (parent.empty()) return base::strConcat(m_package_id.strView(), ".", own_name);

		const auto parent_id = parent.value().unlock(ctx).getID();
		return base::strConcat(getModuleRef(parent_id)->humanReadableID(ctx), ".", own_name);
	}

	std::string ModuleTree::prettyPrint(u32 indentation) const {
		std::stringstream output;

		std::string indent;
		for (u32 i = 0; i < indentation % 3; i++) indent += " ";
		for (u32 i = 0; i < indentation - (indentation % 3); i++)
			indent += (i % 3 == 0 ? "│" : " ");

		if (getName().isBad())
			output << indent << "/ [id: " << reinterpret_cast<u64>(this) << "]\n";
		else
			output << indent << getName().strView() << "/ [name: " << getName().strView() << "]\n";

		if (hasMainSourceFile())
			output << indent << "├> "
				   << getFileRef(getMainSourceFile().illegalAccess().getID())->file.name() << '\n';
		else
			output << indent << "├> Missing main module file!\n";


		for (const auto& [ext, files]: getOtherFiles())
			for (const auto& file: files) output << indent << "├─ " << file.name() << '\n';

		for (const auto& submodule_ref: getSubmodules().illegalAccess())
			output << getModuleRef(submodule_ref.illegalAccess().getID())
						  ->prettyPrint(indentation + 3);

		return output.str();
	}

	void ModuleTree::invalidateHash() {
		// If ModuleHash is invalid, then children are also invalid
		if (!m_path_component_hash.has_value()) {
			// m_hash should not have value if path component hash is invalid
			// The are calculated in the same function: updateModuleHash()
			CORE_ASSERT(!m_hash.has_value(), "Module hash have value!");

			// assert if children are invalid too

			for (const auto& source_file: collectOwnedSourceFiles())
				CORE_ASSERT(
					!source_file->component_hash.has_value(), "Child component hash have value!"
				);
			if (auto submodules = submodulesSlot(); submodules != nullptr)
				for (auto& [_, submodule]: *submodules)
					CORE_ASSERT(
						!submodule->m_path_component_hash.has_value(),
						"Child component hash have value!"
					);
			return;
		}
		m_path_component_hash.reset();
		m_hash.reset();
		for (const auto& source_file: collectOwnedSourceFiles())
			source_file->invalidateComponentHash();
		if (auto submodules = submodulesSlot(); submodules != nullptr)
			for (auto& [_, submodule]: *submodules) submodule->invalidateHash();
	}

	void ModuleTree::updateModuleHash() {
		// Component hash part
		// Get parent component hash if existsS
		if (m_parent.has_value()) {
			{
				IF_BUILD_TYPE_DEV(
					std::scoped_lock parent_lock(*m_parent.value()->m_hash_recompute_mutex);
					CORE_ASSERT(
						m_parent.value()->m_path_component_hash.has_value(),
						"Parent component hash should have value!"
					);
				);
			}
			m_path_component_hash.emplace(m_parent.value()->m_path_component_hash.value(), m_name);
		} else {
			// root module tree, use package id as base
			CORE_ASSERT(m_package_id.isGood(), "Package ID must be set for module tree!");
			m_path_component_hash.emplace(hashing::ComponentHash(m_package_id), m_name);
		}

		// Module hash part

		// @TODO: #1389 verify it

		// Copy the path component hash to the component hash
		hashing::ComponentHash::HashAlg partial = m_path_component_hash->partial;

		// If a Module has parent
		hashing::addToHash(partial, m_parent.has_value());

		// If a Module has a main source file
		hashing::addToHash(partial, hasMainSourceFile());

		// Module kind affects module semantics and therefore must affect module hash.
		hashing::addToHash(partial, static_cast<usize>(getKind()));

		if (isReplModule()) {
			auto repl_module_parent = getReplModuleParent();
			hashing::addToHash(partial, repl_module_parent.has_value());
			if (repl_module_parent.has_value()) {
				auto repl_parent = repl_module_parent.value();

				hashing::addToHash(partial, ModuleTree::getModuleHash(repl_parent));
			}
		}

		// We do not need to add source files count or submodule count here,
		// Because there is a separate SideInput for that
		// And there is no other way to get those counts
		// Only by calling unlock on SourceFilesAccessLocked or SubmodulesAccessLocked

		// We do not need to add a module name and package_name, since they are already in the path
		// component hash

		m_hash = partial.finalize();
	}

	void ModuleTree::updateModuleHashFromRootToThis() {
		std::scoped_lock lock(*m_hash_recompute_mutex);
		if (!m_path_component_hash.has_value()) {
			// iterate thru parents to find one with component hash set or reach root (go up)
			if (m_parent.has_value()) m_parent.value()->updateModuleHashFromRootToThis();
			// go from top module to bottom module, so its in linear time
			updateModuleHash();
		}
	}

	Ref<ModuleTree> ModuleTree::addModuleToStorage(Box<ModuleTree> module) {
		const auto      storage_key  = next_module_storage_key++;
		auto            inserted     = modules.put(storage_key, std::move(module));
		Ref<ModuleTree> module_ref   = inserted->value.get();
		module_ref->m_storage_handle = storage_key;
		return module_ref;
	}

	void ModuleTree::removeModuleFromStorage(Ref<ModuleTree> module) {
		CORE_ASSERT(
			module->m_storage_handle.has_value(),
			"Attempted to remove ModuleTree without storage handle"
		);
		const auto storage_key = module->m_storage_handle.value();
		const bool erased      = modules.erase(storage_key);
		CORE_ASSERT(erased, "Failed to remove ModuleTree from storage");
	}

	void ModuleTree::checkDanglingReference([[maybe_unused]] const base::Ref<ModuleTree>& candidate
	) {
		IF_BUILD_TYPE_DEV({
			// If we are not using module modifier, skip the check
			if (!use_module_modifier_remove) return;
			const auto* candidate_ptr = candidate.get();
			bool        is_tracked    = false;
			for (const auto& entry: modules) {
				if (entry.value.get() == candidate_ptr) {
					is_tracked = true;
					break;
				}
			}
			if (!is_tracked) CORE_PANIC("dangling reference used after removing ModuleTree");
		});
	}

	// ----------------------

	base::StrID moduleName(ModuleID module) { return GetModuleID_Functor::get(module)->getName(); }

	std::string printModuleTree(ModuleID module) {
		return GetModuleID_Functor::get(module)->prettyPrint();
	}

	ModuleID createModuleTree(const fs::File& file, base::StrID package_id) {
		return ModuleTreeBuilder::create(file, package_id)->getModuleID();
	}

	void parseAllFilesInModuleTree(ModuleID module_id) {
		CORE_ASSERT(
			!query::Context::isAnyQueryCurrentlyRunning(),
			"parseAllFilesInModuleTree called when some query is currently running."
		);

		// First collect all files to be parsed.
		std::vector<FileID> files_to_parse;
		auto                collect_files = [&](this auto&& self, ModuleID mid) -> void {
            auto module_tree = GetModuleID_Functor::get(mid);
            if (module_tree->hasMainSourceFile())
                files_to_parse.push_back(module_tree->getMainSourceFile().illegalAccess().getID());
            for (const auto& submodule: module_tree->getSubmodules().illegalAccess())
                self(submodule.illegalAccess().getID());
		};
		collect_files(module_id);

		if (files_to_parse.empty()) return;

		// Now parse them concurrently.
		std::mutex              wait_mtx;
		std::condition_variable wait_cv;
		std::atomic<usize>      next_file_id{ 0 };
		std::atomic<usize>      parsed_files_count{ 0 };
		bool                    all_files_parsed = false;
		auto&                   manager          = concurrent::worker::WorkerManager::get();

		auto schedule_next_file_parsing = [&](concurrent::worker::WRef worker) {
			usize idx = next_file_id.fetch_add(1, std::memory_order_relaxed);

			if (idx < files_to_parse.size()) {
				FileID file_id = files_to_parse[idx];

				worker->scheduleTask([&, file_id](concurrent::worker::WRef) {
					auto file_ref
						= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
							file_id
						);
					file_ref->getPST();

					if (parsed_files_count.fetch_add(1, std::memory_order_relaxed) + 1
					    == files_to_parse.size()) {
						std::lock_guard<std::mutex> lock(wait_mtx);
						all_files_parsed = true;
						wait_cv.notify_one();
					}
				});
			}
		};

		manager.setNoTasksCallback(schedule_next_file_parsing);

		for (auto& worker: manager.getAllWorkers()) schedule_next_file_parsing(worker);

		// Wait until all files are parsed.
		{
			std::unique_lock lock(wait_mtx);
			wait_cv.wait(lock, [&] { return all_files_parsed; });
		}

		// Clear the callback so no worker schedules new parsing tasks.
		manager.setNoTasksCallback([](concurrent::worker::WRef) {});

		// All the lambdas above capture this function's stack frame by reference. A worker that
		// finished the last task may still be re-entering its loop and is about to run the
		// (now-cleared) no-tasks callback, or may have already copied the previous callback before
		// we cleared it (see Worker::run). Either way it would dereference references into this
		// frame. Wait until every worker is idle before returning, otherwise that frame gets
		// destroyed underneath them -> rare segfault.
		manager.waitForAllWorkersFree();
	}

	ModuleID createModuleTreeWithRandomPackageID(const fs::File& file) {
		return ModuleTreeBuilder::createWithRandomPackageID(file)->getModuleID();
	}

	ModuleID createModuleTreeFromContents(
		std::string_view contents, base::Optional<base::StrID> package_id
	) {
		// createModuleTree function expects .dk extension.
		auto virtual_file = fs::FileManager::createRandomVirtualFile(contents, LANG_MODULE_FILE);

		if (!package_id.has_value())
			return createModuleTreeWithRandomPackageID(virtual_file);
		else
			return createModuleTree(virtual_file, package_id.value());
	}

	/**********************
	 * QueryIsReplModule *
	 **********************/
	struct IMPLEMENT_QUERY(QueryIsReplModule, bool) {
		static auto provide(Context&, QKey key) -> PResult {
			// @TODO: #1389 verify Functor correctness.
			return GetModuleID_Functor::get(key)->isReplModule();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryIsReplModule);

	/***************************
	 * QueryReplModuleParent *
	 ***************************/
	struct IMPLEMENT_QUERY(QueryReplModuleParent, base::Optional<ModuleID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// @TODO: #1389 verify Functor correctness.
			auto module_tree = GetModuleID_Functor::get(key);
			if (!module_tree->isReplModule()) return {};
			auto repl_parent = module_tree->getReplModuleParent();
			if (repl_parent.has_value()) {
				// Register dependency on the parent module contents so incremental rebuilds propagate
				ctx.query<QueryModuleSideInput>(KeyOf_ModuleSideInput{
					ModuleTree::getModuleHash(repl_parent.value()) });
			}
			return repl_parent;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReplModuleParent);

	/*********************
	 * QueryParentModule *
	 *********************/
	struct IMPLEMENT_QUERY(QueryParentModule, base::Optional<ModuleID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto module_tree = GetModuleID_Functor::get(key);
			auto parent      = module_tree->getParentModule();
			if (parent) return parent->unlock(ctx).getID();
			return {};
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryParentModule);

	/************************
	 * QueryPackageOfModule *
	 ************************/
	struct IMPLEMENT_QUERY(QueryPackageOfModule, packages::PackageAccessLocked) {
		static auto provide(Context&, QKey key) -> PResult {
			// @TODO: #3505 - currently changing package name/version doesn't invalidate stuff that
			// depends on those values
			return GetModuleID_Functor::get(key)->getPackage();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryPackageOfModule);

	/***********************
	 * QueryMainSourceFile *
	 ***********************/
	struct IMPLEMENT_QUERY(QueryMainSourceFile, FileID) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto module_tree = GetModuleID_Functor::get(key);
			auto file        = module_tree->getMainSourceFile();
			return file.unlock(ctx).getID();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMainSourceFile);

	/*******************
	 * QuerySubmodules *
	 *******************/
	struct IMPLEMENT_QUERY(QuerySubmodules, base::HashMap<base::StrID COMMA ModuleID>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			const auto& module_tree = GetModuleID_Functor::get(key);

			PResult out{};
			for (const auto& module: module_tree->getSubmodules().unlock(ctx)) {
				auto module_id  = module.unlock(ctx).getID();
				auto module_ref = getModuleRef(module_id);
				out.put(module_ref->getName(), module_id);
			}
			return out;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySubmodules);

	/****************
	 * getFilePST *
	 ****************/
	CRef<pst::PST<>> getFilePST([[maybe_unused]] ::query::Context& ctx, FileID file_id) {
		Ref<SourceFile> file
			= GetFileID_Functor::getFileRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				file_id
			);
		return file->getPST();
	}

}
