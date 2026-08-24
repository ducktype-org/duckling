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
	 * The bytes are the ones the `ser` module produces: the whole structure is aggregates,
	 * and the single choice in it (SourcePosition) carries its own hook. Nothing is
	 * reordered on the way out - the offset-keyed vectors are written in the order the
	 * builder produced them, which for one input is the order lowering emits.
	 *
	 * The same input therefore still gives the same file, and that comes from the pipeline
	 * being deterministic rather than from a canonical form imposed here: a module's debug
	 * info is built once per compilation, and the package file merges the per-module ones
	 * into `functions` and `types`, which are ordered maps keyed by name and so do not
	 * depend on the order the modules were compiled in.
	 *
	 * @param info The DebugInfo to serialize.
	 * @param out  Any std::ostream to write to (e.g. a file stream).
	 */
	void saveToStream(const DebugInfo& info, std::ostream& out);

}  // namespace debug_info
