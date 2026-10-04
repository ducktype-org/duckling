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
