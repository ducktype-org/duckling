// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "scope_id.hpp"

#include <helios_private/scopes/scope_data.hpp>

namespace compiler::helios {
	u64 ScopeID::queryUnstablePerfectHash() const { return ref->unstable_id.asInt(); }

	bool ScopeID::operator==(const ScopeID& other) const {
		return queryUnstablePerfectHash() == other.queryUnstablePerfectHash();
	}

	bool ScopeID::operator<(const ScopeID& other) const {
		return queryUnstablePerfectHash() < other.queryUnstablePerfectHash();
	}

	std::strong_ordering ScopeID::operator<=>(const ScopeID& other) const {
		return queryUnstablePerfectHash() <=> other.queryUnstablePerfectHash();
	}
}
