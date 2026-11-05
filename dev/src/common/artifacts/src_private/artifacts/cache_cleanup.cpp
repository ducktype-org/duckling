// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "cache_cleanup.hpp"

#include "cache_files.hpp"
#include "owned_list.hpp"

#include <filesystem/file.hpp>
#include <logger/logger.hpp>

#include <map>
#include <set>

namespace artifacts::detail {
	bool removeOrWarn(const fs::FilePath& path) {
		const auto removed = fs::FileManager::removeEntry(path);
		if (removed.has_value()) return true;
		CORE_USER_LOG("Warning: failed to remove stale artifact entry ", removed.error(), "\n");
		return false;
	}

	std::vector<fs::FilePath> findTempFiles(
		const fs::FilePath& dir, const std::vector<std::string>& names
	) {
		std::map<fs::FilePath, std::set<std::string>> targets_by_folder;
		for (const auto& name: names) {
			const auto path = dir / fs::FilePath(name);
			targets_by_folder[path.parentPath()].insert(path.fileName());
		}

		std::vector<fs::FilePath> temp_files;
		for (const auto& [folder, targets]: targets_by_folder) {
			const auto entries = fs::File::open(folder).and_then([](const fs::File& file) {
				return file.listFilePaths();
			});
			if (!entries.has_value()) continue;
			for (const auto& entry: entries.value()) {
				const auto target = fs::FileManager::temporaryFileTarget(entry);
				if (target.has_value() && targets.contains(target.value().fileName()))
					temp_files.push_back(entry);
			}
		}
		return temp_files;
	}

	bool removeOwnedContents(const fs::FilePath& dir) {
		bool                     all_removed   = true;
		std::vector<std::string> written_names = cacheFileNames(dir);
		for (const auto& entry: readOwnedList(dir).copyValueOrDefault()) {
			if (!isOwnable(dir, entry.name)) continue;
			if (entry.kind == OWNED_FILE) written_names.push_back(entry.name);
			const auto path = dir / fs::FilePath(entry.name);
			if (!path.exists()) continue;

			if (entry.kind == OWNED_COLLECTION) {
				if (!path.isDirectory()) continue;
				all_removed &= removeOwnedContents(path);
				// Fails if something is left inside - which is not ours, so the folder stays.
				std::ignore = fs::FileManager::removeEntry(path);
			} else if (!path.isDirectory()) {
				all_removed &= removeOrWarn(path);
			}
		}

		// Whatever killed writes left of the artifacts and of the cache's own files.
		for (const auto& temp_file: findTempFiles(dir, written_names)) removeOrWarn(temp_file);

		for (int i = 0; i < static_cast<int>(CacheFile::COUNT); i++) {
			// The list goes last: while anything it names is still there, it has to stay.
			if (CacheFile(i) == CacheFile::OwnedList) continue;
			all_removed &= removeOrWarn(cacheFilePath(dir, CacheFile(i)));
		}
		if (all_removed) all_removed = removeOrWarn(cacheFilePath(dir, CacheFile::OwnedList));
		return all_removed;
	}
}
