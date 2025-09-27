/**
 * @file abstract_type.cpp
 * @brief Implementation of AbstractType.
 */

#include "abstract_type.hpp"

#include "internal/abstract_type_impl.hpp"

namespace tsh {
	[[nodiscard]]
	Kind AbstractType::getKind() const {
		return pimpl->getKind();
	}

	[[nodiscard]]
	const TypeInterface& AbstractType::getInterface(query::Context& ctx) const {
		return pimpl->getInterface(ctx);
	}

	[[nodiscard]]
	bool AbstractType::hasNoOpDestructor() const {
		return pimpl->hasNoOpDestructor();
	}

	[[nodiscard]]
	bool AbstractType::isImplicitlyCoercible(const AbstractType target, query::Context& ctx) const {
		return pimpl->isImplicitlyCoercible(target, ctx);
	}

	[[nodiscard]]
	const std::string& AbstractType::toString() const {
		return pimpl->toString();
	}

	u64 AbstractType::queryUnstablePerfectHash() const { return u64(pimpl); }

	// Specialized template definition and explicit instantiation.
	template<>
	AbstractType::CPimpl checkDynamicCast<AbstractType>(AbstractType::CPimpl p) {
		return p;
	}
}
