// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <filesystem/file_path.hpp>

#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace artifacts::detail {
	/**
	 * @brief The cache's own bookkeeping files: everything a collection writes besides its
	 * artifacts.
	 *
	 * Every such file is named through `cacheFilePath`, and the cleanup goes over all values of
	 * this enum, so a file added here cannot be left out of the cleanup by accident.
	 */
	enum class CacheFile { Artc, OwnedList, BuildId, COUNT };

	/// Where `file` lives for the collection kept in `dir`.
	[[nodiscard]] fs::FilePath cacheFilePath(const fs::FilePath& dir, CacheFile file);

	/// Names of all the cache's own files (see `CacheFile`) in `dir`.
	[[nodiscard]] std::vector<std::string> cacheFileNames(const fs::FilePath& dir);

	/**
	 * @brief Tells the user that the artifacts cache could not be saved, and why.
	 *
	 * Losing the cache costs the next build its incremental speed-up, nothing more: the artifacts
	 * this build produced are already on disk. So the user is told and the build is allowed to
	 * finish - but told loudly, because silence here used to be how a full disk turned into a
	 * mysteriously slow or broken project.
	 */
	void reportCacheWriteFailure(std::string_view reason);

	/// Writes one of the cache's own files atomically; a failure is reported, not fatal.
	void writeCacheFile(
		const fs::FilePath& dir, CacheFile file, const std::function<void(std::ostream&)>& write
	);
}
