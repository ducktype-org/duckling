// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "debug_info.hpp"

#include <expected>
#include <iosfwd>
#include <string>

namespace debug_info {

	/**
	 * @brief Deserializes a DebugInfo object from a JSON stream.
	 *
	 * On success returns the deserialized DebugInfo.
	 * On failure (malformed JSON, missing fields, wrong types, …) returns an
	 * error string describing the problem — no exception is thrown.
	 *
	 * @param in Any std::istream containing a JSON-encoded DebugInfo.
	 */
	std::expected<DebugInfo, std::string> loadFromStream(std::istream& in);

	/**
	 * @brief Serializes a DebugInfo object to a JSON stream.
	 *
	 * The output is indented JSON (4-space indent) for human readability.
	 * Instruction entries within each function are sorted by offset before
	 * being written.
	 *
	 * @param info The DebugInfo to serialize.
	 * @param out  Any std::ostream to write to (e.g. a file stream).
	 */
	void saveToStream(const DebugInfo& info, std::ostream& out);

}  // namespace debug_info
