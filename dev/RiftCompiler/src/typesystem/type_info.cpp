/**
 * @file type_info.cpp
 * @brief Implementation of TypeInfo.
 *
 * This file is not included outside the Type System module and can thus have full knowledge of the
 * underlying implementation hierarchy.
 */

#include "type_info.hpp"

#include "internal/type_info_impl.hpp"

namespace ts {
	[[nodiscard]]
	Kind TypeInfo::getKind() const {
		return pimpl->getKind();
	}

	[[nodiscard]]
	usize TypeInfo::getSize() const {
		return pimpl->getSize();
	}

	[[nodiscard]]
	const std::string& TypeInfo::show() const {
		return pimpl->show();
	}

	[[nodiscard]]
	bool TypeInfo::isInfoImplicitlyCoercible(const TypeInfo to) const {
		return pimpl->isInfoImplicitlyCoercible(to);
	}

	// Specialized template definition and explicit instantiation.
	template<>
	TypeInfo::CPimpl checkDynamicCast<TypeInfo>(TypeInfo::CPimpl p) {
		return p;
	}

	template TypeInfo::CPimpl checkDynamicCast<TypeInfo>(TypeInfo::CPimpl);
}
