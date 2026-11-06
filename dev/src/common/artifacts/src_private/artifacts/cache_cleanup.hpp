// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <filesystem/file_path.hpp>

#include <string>
#include <vector>

namespace artifacts::detail {
	/**
	 * @brief Removes `path` - the entry itself, never a symlink's target - warning if that fails.
	 * @return False if `path` could not be removed; a missing `path` counts as removed.
	 */
	bool removeOrWarn(const fs::FilePath& path);

	/**
	 * @brief Finds the temporary files a killed write left of the files `names` in `dir`.
	 *
	 * Every file the cache writes - an artifact or one of its own - is written to
	 * `<name>.<random>.tmp` next to it and renamed into place (see
	 * `fs::FileManager::produceFileAtomically`), so such a file belongs to whoever owns `<name>`.
	 *
	 * @param names Paths relative to `dir`; a name like `bin/app` is looked for in `dir/bin`.
	 */
	[[nodiscard]] std::vector<fs::FilePath> findTempFiles(
		const fs::FilePath& dir, const std::vector<std::string>& names
	);

	/**
	 * @brief Removes what the cache owns in `dir`, and nothing else: the entries its owned list
	 * names (descending into owned sub-collections), then the cache's own files and whatever
	 * temporary files a killed write left of them.
	 *
	 * @return False if something owned could not be removed. The list is then kept, so the
	 * next cleanup still knows about it.
	 */
	bool removeOwnedContents(const fs::FilePath& dir);
}
