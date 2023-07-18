/**
 * @file vtable_creation.hpp
 * @brief creates and fills CTVs holding vtable
 */

#pragma once

#include "ctv.hpp"

#include <iostream>
#include <map>
#include <typesystem/class_types.hpp>


namespace exec {

	namespace internal {
		static std::map<std::pair<ts::ClassInfo, ts::ClassInfo>, CTV> vtables;
	}

	CTV getVtable(ts::ClassInfo parent, ts::ClassInfo child);

	CTV getVirtualSubCtv(CTV ctv, size_t vtable_position, size_t offset,
	                     ts::TypeDesc<> wanted_type);

	void fillVtablePtr(CTV ctv, ts::ClassInfo base_class);

	void fillAllVtablePtrs(CTV ctv);
}
