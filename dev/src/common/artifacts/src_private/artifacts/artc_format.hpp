// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <base/collections/optional.hpp>
#include <base/misc/raw_view.hpp>

#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * The `.artc` format, which keeps a collection's blobs: `<blob count>;` then, for each blob,
 * `<name size>;<name><data size>;<data>`.
 */
namespace artifacts::detail {
	struct StoredBlob final {
		std::string       name;
		std::vector<byte> data;
	};

	/**
	 * @brief Decodes a whole `.artc`.
	 *
	 * A `.artc` can be torn by a write that ran out of disk or a build that was killed, and its
	 * tail then decodes into nonsense. That is a reason to drop the cache, never to bring the
	 * compiler down, so every field is validated - sizes against the bytes that are actually
	 * left, so that a corrupted one cannot become a huge allocation.
	 *
	 * @return All the blobs, or nothing if `content` is not a whole, well-formed `.artc`.
	 */
	[[nodiscard]] base::Optional<std::vector<StoredBlob>> parseArtc(std::string_view content);

	/// Encodes `blobs`, given as name and data, as a `.artc`.
	void writeArtc(
		std::ostream& out, const std::vector<std::pair<std::string_view, base::RawView>>& blobs
	);
}
