// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

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
	CRef<query::QResult<TypeInterface>> AbstractType::getInterfaceResult(query::Context& ctx) const {
		return pimpl->getInterfaceResult(ctx);
	}

	[[nodiscard]]
	bool AbstractType::isSimple() const {
		return pimpl->isSimple();
	}

	[[nodiscard]]
	bool AbstractType::isTriviallyDestructible(query::Context& ctx) const {
		return pimpl->isTriviallyDestructible(ctx);
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
