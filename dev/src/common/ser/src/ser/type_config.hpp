#pragma once

// ── ser::config<T> ────────────────────────────────────────────────────────────
// The per-TYPE policy trait. One member is read today, `schema_id`, and hash.hpp is the
// only reader:
//
//     template <> struct ser::config<Artifact> {
//         static constexpr ::std::uint64_t schema_id = 0xA47F'0001;
//     };
//
// It replaces the derived hash for that type - the answer for a type whose format the
// library cannot see, and the only way to make such a type's hash stable across standard
// libraries.

namespace ser {

	template<class T>
	struct config {};

}  // namespace ser
