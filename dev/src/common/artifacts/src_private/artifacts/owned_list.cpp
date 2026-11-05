// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "owned_list.hpp"

#include "cache_files.hpp"

#include <filesystem/file.hpp>
#include <logger/logger.hpp>

#include <unordered_set>

namespace artifacts::detail {
	namespace {
		/// The first line of every list: says what the file is and its format version.
		constexpr std::string_view OWNED_LIST_HEADER = "duckling-artifacts 1";

		std::string ownedListLine(const OwnedEntry& entry) {
			return std::string{ entry.kind, ' ' } + entry.name + '\n';
		}
	}

	bool staysInsideCollection(std::string_view name) {
		if (name.empty() || name.find('\n') != std::string_view::npos) return false;
		const fs::FilePath path{ std::string(name) };
		if (path.hasRoot() || !path.isRelative()) return false;
		const auto parts = path.lexicallyNormal().parts();
		// After normalization a `..` can only be left at the front.
		return !parts.empty() && parts.front() != ".." && parts.front() != "."
		    && !parts.back().empty();
	}

	bool isOwnable(const fs::FilePath& dir, std::string_view name) {
		if (!staysInsideCollection(name)) return false;
		const fs::FilePath entry{ std::string(name) };
		if (!dir.passesThroughSymlink(entry)) return true;
		CORE_DEV_LOG(
			Artifacts,
			"Skipping artifact entry reached through a symlink: ",
			(dir / entry).string(),
			"\n"
		);
		return false;
	}

	base::Optional<std::vector<OwnedEntry>> readOwnedList(const fs::FilePath& dir) {
		const auto file = fs::File::open(cacheFilePath(dir, CacheFile::OwnedList));
		if (!file.has_value()) return {};
		const auto content = file->getContent();
		if (!content.has_value()) return {};
		std::string_view rest = content->view().stringView();

		const auto header_end = rest.find('\n');
		if (rest.substr(0, header_end) != OWNED_LIST_HEADER) return {};
		rest = header_end == std::string_view::npos ? "" : rest.substr(header_end + 1);

		std::vector<OwnedEntry>         entries;
		std::unordered_set<std::string> seen;
		// Only whole lines: a last line without its newline was cut short by a killed build, and
		// a cut name can be the name of some other file - so it is not trusted.
		for (auto line_end = rest.find('\n'); line_end != std::string_view::npos;
		     rest = rest.substr(line_end + 1), line_end = rest.find('\n')) {
			const auto line        = rest.substr(0, line_end);
			const bool well_formed = line.size() > 2 && line[1] == ' '
			                      && (line[0] == OWNED_FILE || line[0] == OWNED_COLLECTION);
			if (!well_formed) continue;
			auto name = std::string(line.substr(2));
			if (!staysInsideCollection(name) || !seen.insert(name).second) continue;
			entries.push_back({ .kind = line[0], .name = std::move(name) });
		}
		return entries;
	}

	std::string formatOwnedList(const std::vector<OwnedEntry>& entries) {
		std::string content = std::string(OWNED_LIST_HEADER) + '\n';
		for (const auto& entry: entries) content += ownedListLine(entry);
		return content;
	}

	bool appendOwnedEntry(const fs::FilePath& dir, const OwnedEntry& entry) {
		const auto list = cacheFilePath(dir, CacheFile::OwnedList);
		const auto line = ownedListLine(entry);

		// A new list starts with its header. If another process creates the list first, this
		// one appends to that instead.
		if (!list.exists()) {
			const auto created = fs::FileManager::createPhysicalFile(
				list.absolute(), std::string(OWNED_LIST_HEADER) + '\n' + line
			);
			if (created.has_value()) return true;
			if (!list.exists()) {
				reportCacheWriteFailure(created.error());
				return false;
			}
		}

		auto file = fs::File::open(list);
		if (!file.has_value()) {
			reportCacheWriteFailure(file.error());
			return false;
		}
		if (auto written = file->writeToFile(line, /*append=*/true); !written.has_value()) {
			reportCacheWriteFailure(written.error());
			return false;
		}
		return true;
	}
}
