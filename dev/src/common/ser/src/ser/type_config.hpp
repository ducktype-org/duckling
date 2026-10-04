#pragma once

/**
 * @file
 * @brief ser::config<T>, per-type settings. Today it has one: `schema_id`.
 * @details `schema_id` replaces the schema hash computed for T with a fixed number:
 *
 *     template<> struct ser::config<Artifact> {
 *         static constexpr ::std::uint64_t schema_id = 0xA47F'0001;
 *     };
 *
 * It is meant for a type with a hand-written hook. ser cannot see what such a hook writes, so
 * the computed hash of that type is only `"hook"`, its `sizeof` and its `alignof` - and
 * `sizeof(std::string)` is different under libstdc++ and libc++. `schema_id` gives the type a
 * hash that does not depend on any of that. Change the number when the hook's format changes.
 *
 * @TODO: #90006 require an explicit schema for such types instead of the sizeof fallback
 */

namespace ser {

	template<class T>
	struct config {};

} /* namespace ser */
