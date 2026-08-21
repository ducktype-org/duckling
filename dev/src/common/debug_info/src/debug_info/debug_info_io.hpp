#pragma once

#include "debug_info.hpp"

#include <expected>
#include <iosfwd>
#include <string>

namespace debug_info {

	/**
	 * @brief Reads a DebugInfo object back from a binary stream.
	 *
	 * On success returns the DebugInfo. On failure (truncated, not debug info at all, or
	 * entries out of order) returns an error string describing the problem - no exception is
	 * thrown, so a damaged file is something the caller can carry on without.
	 *
	 * @param in Any std::istream containing what saveToStream wrote.
	 */
	std::expected<DebugInfo, std::string> loadFromStream(std::istream& in);

	/**
	 * @brief Writes a DebugInfo object to a binary stream.
	 *
	 * The bytes are the ones the `ser` module produces: the whole structure is aggregates,
	 * and the single choice in it (SourcePosition) carries its own hook. Instruction entries
	 * within each function are sorted by offset before being written, so the same DebugInfo
	 * always produces the same file.
	 *
	 * @param info The DebugInfo to serialize.
	 * @param out  Any std::ostream to write to (e.g. a file stream).
	 */
	void saveToStream(const DebugInfo& info, std::ostream& out);

}  // namespace debug_info
