#pragma once

// ── ser::config<T> ────────────────────────────────────────────────────────────
// The per-TYPE policy trait, next to ser::serializer<T> in spirit: an empty primary
// template that says nothing, and a specialization that says one thing. One member is
// read today, `schema_id`, and hash.hpp is the only reader:
//
//     template <> struct ser::config<Artifact> {
//         static constexpr ::std::uint64_t schema_id = 0xA47F'0001;
//     };
//
// That replaces the derived hash for that type. It is the answer for a type whose format
// the library cannot see (a hand-written serWrite), and the only way to make such a
// type's hash stable across standard libraries.
//
// The primary is EMPTY rather than a set of defaults, so that a specialization naming
// only the member it cares about cannot silently drop the others as members are added.
// Every reader asks with a requires-clause and supplies its own default instead.

namespace ser {

	template<class T>
	struct config {};

}  // namespace ser
