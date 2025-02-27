/**
 * @file abstract_type.cpp
 * @brief Implementation of AbstractType.
 *
 * This file is not included outside the Type System module and can thus have full knowledge of the
 * underlying implementation hierarchy.
 */

#include "abstract_type.hpp"

#include "internal/abstract_type_impl.hpp"
#include "internal/queries.hpp"

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
	bool AbstractType::isImplicitlyCoercible(const AbstractType target, query::Context& ctx) const {
		return pimpl->isImplicitlyCoercible(target, ctx);
	}

	[[nodiscard]]
	const std::string& AbstractType::toString() const {
		return pimpl->toString();
	}

	base::HashT AbstractType::customPerfectHash() const { return base::HashT(pimpl); }

	// Specialized template definition and explicit instantiation.
	template<>
	AbstractType::CPimpl checkDynamicCast<AbstractType>(AbstractType::CPimpl p) {
		return p;
	}
}
