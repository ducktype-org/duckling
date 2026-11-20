// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "symbol_id.hpp"

#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

namespace compiler::helios {

	u64 SymID::queryUnstablePerfectHash() const { return ref->id.asInt(); }

	tsh::ClassAbstractType classMemberOwner(const SymID member) {
		CORE_ASSERT(isClassMember(kind(member)), "Expected a member of a class symbol.");

		auto member_data = getSymRef(member)->getDataOpt<ClassMemberSemantics>();
		CORE_ASSERT(member_data.has_value(), "Expected a class member defined in the PST.");

		return member_data.value()->owner_class;
	}

	bool isStaticField(query::Context& ctx, const SymID symbol) {
		if (kind(symbol) != SymbolKind::Field) return false;

		auto element = typeMemberOwner(symbol).getInterface(ctx)->getElementBySym(symbol);
		return element.has_value() and element.value()->isStaticField();
	}

	tsh::AbstractType typeMemberOwner(const SymID member) {
		variant_match(getSymRef(member)->other) {
			variant_case(ClassMemberSemantics, member_data) { return member_data.owner_class; }
			variant_case(defgen::Field, field) { return field.parent_type; }
			variant_case(defgen::Method, method) { return method.owner_type; }
			variant_case(defgen::Constructor, constructor) { return constructor.type; }
			variant_default {
				CORE_PANIC("Symbol `", name(member).strView(), "` is not a member of a type.");
			}
		}
		CORE_UNREACHABLE();
	}
}
