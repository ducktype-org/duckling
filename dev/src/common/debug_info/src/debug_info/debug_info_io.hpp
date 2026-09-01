#pragma once

#include "debug_info.hpp"

#include <ser/stream/read_write.hpp>

#include <cstdint>
#include <expected>
#include <iosfwd>
#include <string>

namespace debug_info {

	/**
	 * @brief The envelope both directions of the .di format use.
	 *
	 * The one persisted format in the tree that is read OUTSIDE the artifact cache: the VM
	 * debugger opens `duck_build/package_dvm.di` with a plain fstream from a separately
	 * launched binary, so the `.build_id` wipe that protects the query graph and the
	 * metadata blobs never runs for it. Without a header a `.di` from another build, or
	 * another format altogether, decodes as data - which the JSON this replaced could not
	 * do, being self-describing.
	 *
	 * The header carries the magic, the platform flags, `schema_hash`.
	 */
	inline constexpr ::ser::options DI_STREAM{
		.header = true,
		/* 'DINF', little-endian */
		.user_magic = ::std::uint32_t{ 0x46'4E'49'44 },
	};

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
	 * The vectors go out in the order they are held, so the file is insertion-orderedt. The same
	 * input gives the same file, and that comes from the pipeline being deterministic: a module's
	 * debug info is built once per compilation in a deterministic order.
	 *
	 * @param info The DebugInfo to serialize.
	 * @param out  Any std::ostream to write to (e.g. a file stream).
	 */
	void saveToStream(const DebugInfo& info, std::ostream& out);

} /* namespace debug_info */
