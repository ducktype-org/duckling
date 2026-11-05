// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "cache_files.hpp"

#include <base/except/exceptions.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file.hpp>
#include <logger/logger.hpp>

namespace artifacts::detail {
	fs::FilePath cacheFilePath(const fs::FilePath& dir, CacheFile file) {
		switch (file) {
		case CacheFile::Artc:
			return dir / fs::FilePath(dir.fileName() + ".artc");
		case CacheFile::OwnedList:
			return dir / fs::FilePath(std::string(ArtifactCollection::OWNED_LIST_FILE));
		case CacheFile::BuildId:
			return dir / fs::FilePath(std::string(ArtifactCollection::BUILD_ID_FILE));
		default:
			CORE_PANIC("CacheFile has no file name: ", static_cast<int>(file));
		}
	}

	std::vector<std::string> cacheFileNames(const fs::FilePath& dir) {
		std::vector<std::string> names;
		names.reserve(static_cast<usize>(CacheFile::COUNT));
		for (int i = 0; i < static_cast<int>(CacheFile::COUNT); i++)
			names.push_back(cacheFilePath(dir, CacheFile(i)).fileName());
		return names;
	}

	void reportCacheWriteFailure(std::string_view reason) {
		CORE_USER_LOG(
			"Warning: could not save the artifacts cache: ",
			reason,
			". The next build will not be able to reuse it.\n"
		);
	}

	void writeCacheFile(
		const fs::FilePath& dir, CacheFile file, const std::function<void(std::ostream&)>& write
	) {
		const auto written = fs::FileManager::writeFileAtomically(cacheFilePath(dir, file), write);
		if (!written.has_value()) reportCacheWriteFailure(written.error());
	}
}
