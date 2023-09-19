/**
 * @file helpers.hpp
 * @brief helper functions for common class operations on CTVs
 */

#pragma once

#include <exec/ctv.hpp>
#include <exec/exec.hpp>
#include <exec/vtable_creation.hpp>
#include <operations/create_default.hpp>
#include <operations/operation.hpp>
#include <typesystem/typesystem.hpp>

namespace exec {

	inline exec::CTV getVirtualMember(exec::CTV ctv, ts::ClassInfo class_info,
	                                  symtable::SymbolId symbol) {
		ts::MemberInfo member          = class_info.getMemberInfo(symbol);
		auto           member_ancestor = member.last_virtual_ancestor.value();
		auto           member_offset   = member.start_offset.value();
		auto           member_position = class_info.getVtablePositionOf(member_ancestor);
		auto           member_type     = member.desc.value();
		auto member_ctv = exec::getVirtualSubCtv(ctv, member_position, member_offset, member_type);

		return member_ctv;
	}

	inline exec::CTV getMemberNonVirtual(exec::CTV ctv, symtable::SymbolId symbol) {
		ts::ClassInfo class_info = ctv.type.getType();
		auto          member     = class_info.getMemberInfo(symbol);
		auto          offset     = member.start_offset.value();
		auto          desc       = member.desc.value();

		return ctv.subCTV(desc, offset, desc.getType().getSize());
	}

	inline exec::CTV getMember(exec::CTV ctv, symtable::SymbolId symbol, ts::ClassInfo class_info) {
		ts::MemberInfo member = class_info.getMemberInfo(symbol);

		if (member.last_virtual_ancestor.has_value())
			return getVirtualMember(ctv, class_info, symbol);
		return getMemberNonVirtual(ctv, symbol);
	}

	inline exec::CTV getVirtualMember(exec::CTV ctv, ts::ClassInfo class_info,
	                                  ts::MemberInfo member) {
		auto member_ancestor = member.last_virtual_ancestor.value();
		auto member_offset   = member.start_offset.value();
		auto member_position = class_info.getVtablePositionOf(member_ancestor);
		auto member_type     = member.desc.value();
		auto member_ctv = exec::getVirtualSubCtv(ctv, member_position, member_offset, member_type);

		return member_ctv;
	}

	inline exec::CTV getMemberNonVirtual(exec::CTV ctv, ts::MemberInfo member) {
		auto offset = member.start_offset.value();
		auto desc   = member.desc.value();

		return ctv.subCTV(desc, offset, desc.getType().getSize());
	}

	inline exec::CTV getMember(exec::CTV ctv, const ts::MemberInfo& member,
	                           ts::ClassInfo class_info) {
		if (member.last_virtual_ancestor.has_value())
			return getVirtualMember(ctv, class_info, member);
		return getMemberNonVirtual(ctv, member);
	}
}
