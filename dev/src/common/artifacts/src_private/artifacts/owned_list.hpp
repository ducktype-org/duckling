// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

#include <string>
#include <string_view>
#include <vector>

/**
 * The list of what a collection owns in its folder - see `ArtifactCollection::OWNED_LIST_FILE`.
 */
namespace artifacts::detail {
	/// An entry for a file artifact.
	constexpr char OWNED_FILE = 'f';
	/// An entry for a sub-collection.
	constexpr char OWNED_COLLECTION = 'c';

	struct OwnedEntry final {
		char        kind;
		std::string name;
	};

	/**
	 * @brief Answers, for a relative path such as an entry name from the `.duck_artifacts`
	 * list: once joined to any base folder, does this path name something strictly inside it?
	 *
	 * The cleanup only ever touches the folder the user gave as the artifacts folder, so a name
	 * that leads out of it - absolute, or through `..` - is never owned: such a file is written
	 * where it was asked for, but the cache does not remove it later. A name that leads back to
	 * the folder itself, like `.`, would make a collection its own sub-collection and recurse
	 * forever. The check is on the normalized name, so `a/../b` and `./b` are judged as `b`.
	 */
	[[nodiscard]] bool staysInsideCollection(std::string_view name);

	/**
	 * @brief Whether the entry `name` in `dir` is one the cache may load, record and remove: it
	 * stays inside the collection and is not reached through a symlink.
	 *
	 * The compiler never creates symlinks in the cache, so an entry reached through one is not
	 * ours: it is skipped, the same way the module tree builder skips symlinks among source
	 * files.
	 */
	[[nodiscard]] bool isOwnable(const fs::FilePath& dir, std::string_view name);

	/**
	 * @brief Reads the entries of `dir`'s list.
	 * @return Nothing if the list is missing or is not one we know how to read - then `dir` is
	 * not a collection of ours.
	 */
	[[nodiscard]] base::Optional<std::vector<OwnedEntry>> readOwnedList(const fs::FilePath& dir);

	/// The whole content of a list holding `entries`.
	[[nodiscard]] std::string formatOwnedList(const std::vector<OwnedEntry>& entries);

	/**
	 * @brief Appends one entry to `dir`'s list, creating the list if there is none.
	 *
	 * Appending, not rewriting: each entry is one short write, so entries that concurrent
	 * processes append to the same list do not overwrite each other.
	 *
	 * @return False if the entry could not be written; that is reported to the user.
	 */
	bool appendOwnedEntry(const fs::FilePath& dir, const OwnedEntry& entry);
}
