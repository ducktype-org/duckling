#pragma once

// ── ser::config<T> ────────────────────────────────────────────────────────────
// The per-TYPE policy trait, next to ser::serializer<T> in spirit: an empty primary
// template that says nothing, and a specialization that says one thing.
//
// M1 reads exactly one member out of it, `schema_id`, and hash.hpp is the only reader:
//
//     template <> struct ser::config<Artifact> {
//         static constexpr ::std::uint64_t schema_id = 0xA47F'0001;
//     };
//
// That replaces the derived hash for that type - see the note on schema_hash. It is the
// answer for a type whose format the library cannot see (a hand-written serWrite), and
// the only way to make such a type's hash stable across standard libraries.
//
// The primary is EMPTY rather than a set of defaults, and that matters for what comes
// later: M2/M3 add `flat` and `no_padding` here, and a user specialization written today
// names only the member it cares about. If the primary carried defaults, that
// specialization would silently drop them - so every reader asks with a requires-clause
// and supplies its own default instead.

namespace ser {

	template<class T>
	struct config {};

}  // namespace ser
