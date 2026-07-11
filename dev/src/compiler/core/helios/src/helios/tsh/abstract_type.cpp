/**
 * @file abstract_type.cpp
 * @brief Implementation of AbstractType.
 */

#include "abstract_type.hpp"

#include <helios_private/tsh/abstract_type_impl.hpp>

namespace compiler::tsh {
	[[nodiscard]]
	Kind AbstractType::getKind() const {
		return pimpl->getKind();
	}

	[[nodiscard]]
	CRef<TypeInterface> AbstractType::getInterface(query::Context& ctx) const {
		return pimpl->getInterface(ctx);
	}

	[[nodiscard]]
	bool AbstractType::isSimple() const {
		return pimpl->isSimple();
	}

	[[nodiscard]]
	bool AbstractType::hasNoOpDestructor() const {
		return pimpl->hasNoOpDestructor();
	}

	[[nodiscard]]
	bool AbstractType::isDefaultConstructible(query::Context& ctx) const {
		return pimpl->isDefaultConstructible(ctx);
	}

	[[nodiscard]]
	bool AbstractType::isTriviallyZeroInitializable(query::Context& ctx) const {
		return pimpl->isTriviallyZeroInitializable(ctx);
	}

	[[nodiscard]]
	bool AbstractType::isCopyable(query::Context& ctx) const {
		return pimpl->isCopyable(ctx);
	}

	[[nodiscard]]
	bool AbstractType::isTriviallyCopyable(query::Context& ctx) const {
		return pimpl->isTriviallyCopyable(ctx);
	}

	[[nodiscard]]
	bool AbstractType::isImplicitlyCoercible(const AbstractType target, query::Context& ctx) const {
		return pimpl->isImplicitlyCoercible(target, ctx);
	}

	bool AbstractType::carriesInformation(query::Context& ctx) const {
		return pimpl->carriesInformation(ctx);
	}

	[[nodiscard]]
	const std::string& AbstractType::toString() const {
		return pimpl->toString();
	}

	u64 AbstractType::queryUnstablePerfectHash() const { return u64(pimpl.get()); }

	// Specialized template definition and explicit instantiation.
	template<>
	AbstractType::CPimpl checkDynamicCast<AbstractType>(AbstractType::CPimpl p) {
		return p;
	}
}
