/**
 * @file type_info.cpp
 * @brief Implementation of TypeInfo.
 *
 * This file is not included outside the Type System module and can thus have full knowledge of the
 * underlying implementation hierarchy.
 */

#include "type_info.hpp"

#include "internal/type_info_impl.hpp"
#include "internal/queries.hpp"

namespace tsh {
	[[nodiscard]]
	Kind TypeInfo::getKind() const {
		return pimpl->getKind();
	}

	[[nodiscard]]
	const TypeInterface& TypeInfo::getInterface(query::Context& ctx) const {
		return pimpl->getInterface(ctx);
	}

	[[nodiscard]]
	bool TypeInfo::isImplicitlyCoercible(const TypeInfo target, query::Context& ctx) const {
		return pimpl->isImplicitlyCoercible(target, ctx);
	}

	[[nodiscard]]
	const std::string& TypeInfo::toString() const {
		return pimpl->toString();
	}

	base::HashT TypeInfo::customPerfectHash() const { return base::HashT(pimpl); }

	// Specialized template definition and explicit instantiation.
	template<>
	TypeInfo::CPimpl checkDynamicCast<TypeInfo>(TypeInfo::CPimpl p) {
		return p;
	}
}
