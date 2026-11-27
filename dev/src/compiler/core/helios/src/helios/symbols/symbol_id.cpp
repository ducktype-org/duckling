// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "symbol_id.hpp"

#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>

namespace compiler::helios {

	u64 SymID::queryUnstablePerfectHash() const { return ref->id.asInt(); }

	tsh::ClassAbstractType classMemberOwner(const SymID member) {
		CORE_ASSERT(isClassMember(kind(member)), "Expected a member of a class symbol.");
		return typeMemberOwner(member).as<tsh::ClassAbstractType>();
	}

	bool isStaticField(query::Context& ctx, const SymID symbol) {
		if (kind(symbol) != SymbolKind::Field) return false;

		auto element = typeMemberOwner(symbol).getInterface(ctx)->getElementBySym(symbol);
		return element.has_value() and element.value()->isStaticField();
	}

	tsh::AbstractType typeMemberOwner(const SymID member) {
		if_opt_some(getSymRef(member)->maybe_type_member_data, member_data) {
			return member_data.owner_type;
		}
		CORE_PANIC("Symbol `", name(member).strView(), "` is not a member of a type.");
	}
}
