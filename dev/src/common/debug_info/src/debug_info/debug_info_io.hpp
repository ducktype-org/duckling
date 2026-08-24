#pragma once

#include "debug_info.hpp"

#include <expected>
#include <iosfwd>
#include <string>

namespace debug_info {

	/**
	 * @brief Reads a DebugInfo object back from a binary stream.
	 *
	 * On success returns the DebugInfo. On failure (truncated, or not debug info at all)
	 * returns an error string describing the problem - no exception is thrown, so a damaged
	 * file is something the caller can carry on without.
	 *
	 * @param in Any std::istream containing what saveToStream wrote.
	 */
	std::expected<DebugInfo, std::string> loadFromStream(std::istream& in);

	/**
	 * @brief Writes a DebugInfo object to a binary stream.
	 *
	 * Uses ser library to write a canonical form of the debug info.
	 *
	 * The same input therefore still gives the same file, and that comes from the pipeline
	 * being deterministic rather than from a canonical form imposed here: a module's debug
	 * info is built once per compilation in a deterministic order.
	 *
	 * @param info The DebugInfo to serialize.
	 * @param out  Any std::ostream to write to (e.g. a file stream).
	 */
	void saveToStream(const DebugInfo& info, std::ostream& out);

}  // namespace debug_info
