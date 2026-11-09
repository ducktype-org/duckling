// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <cstdint>

namespace pst {
	/**
	 * @brief Describes what `AccessLocked::unlock` of a PST element reports to the query framework.
	 *
	 * PST elements are grouped into "unlock groups": a group is a `Full` element together with all
	 * its `Transparent` descendants (the sub-tree cut at other `Full` elements). Each group is a
	 * single query input, identified by the group hash: the hash of the hashes of all the group
	 * elements. Unlocking any element of a group registers the group as a dependency.
	 *
	 * The mode is a per-element property set in element constructors (so it can also be decided
	 * during parsing). Changing an element type between Full and Transparent should only require
	 * changing the line that sets it in the element constructor.
	 *
	 * @note Element hashes encode the structure of the element (e.g. the number of children), so a
	 * group hash also changes when a Full child is added or removed.
	 */
	enum class UnlockMode : uint8_t {
		/**
		 * The element is the root of an unlock group: unlocking it registers its group as a PST
		 * access side input. The root of every input PST is Full.
		 */
		Full,

		/**
		 * The element belongs to the unlock group of its nearest Full ancestor: unlocking it
		 * registers that group.
		 */
		Transparent,

		/**
		 * The element belongs to a generated PST (macro expansion, baked template).
		 *
		 * Generated PSTs never exist in source files, so they can't be inputs of incremental
		 * compilation. Their content is fully determined by the query that generated them (all
		 * elements are signed with the hash of the whole generated tree), so unlocking them is a
		 * no-op. The dependency is carried by the generating query (and by query keys built from
		 * the signed hashes).
		 */
		Generated,
	};
}
