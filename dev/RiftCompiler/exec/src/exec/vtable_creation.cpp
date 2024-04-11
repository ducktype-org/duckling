#include "vtable_creation.hpp"

namespace exec {
	CTV getVtable(ts::ClassInfo parent, ts::ClassInfo child) {
		if (internal::vtables.contains({ parent, child }))
			return internal::vtables.at({ parent, child });

		auto table = child.getVirtualAncestorTable(parent);

		auto ctv
			= alloc_new(query::queryEntryPoint<ts::QueryIntegralType>({ 64 }), parent.getVtableSize() * sizeof(usize) * 8);
		//							^^^ TO UWZGLĘDNIA METODY WIRTUALNE

		for (usize i = 0; i < table.size(); i++) ctv.getData<usize>()[i] = table[i].second;

		internal::vtables.insert({ { parent, child }, ctv });

		return ctv;
	}

	CTV getVirtualSubCtv(CTV ctv, usize vtable_position, usize offset, ts::TypeDesc<> wanted_type) {
		ts::ClassInfo baseClass         = ctv.type.getType();
		usize         vtable_ptr_offset = baseClass.getVtablePtrOffset();

		auto vtable_ptr_ctv = ctv.subCTV(
			query::queryEntryPoint<ts::QueryPointerType>({ts::VTableInfo::create(baseClass), false}),
			vtable_ptr_offset,
			ts::POINTER_SIZE
		);

		auto ancestor_offset = vtable_ptr_ctv.getDataUnderPointer<usize>()[vtable_position];

		return ctv.subCTV(wanted_type, ancestor_offset + offset, wanted_type.getType().getSize());
	}

	void fillVtablePtr(CTV ctv, ts::ClassInfo base_class) {
		ts::ClassInfo our_info = ctv.type.getType();
		if (our_info.getVtableSize() > 0) {
			usize vtable_ptr_offset = our_info.getVtablePtrOffset();
			auto  vtable_ptr_ctv
				= ctv.subCTV(query::queryEntryPoint<ts::QueryRawPointerType>({}), vtable_ptr_offset, ts::POINTER_SIZE);

			// @TODO: Make this a normal memcopy
			vtable_ptr_ctv.getData<usize>().front()
				= getVtable(our_info, base_class).makePointer().getData<usize>().front();
		}
	}

	void fillAllVtablePtrs(CTV ctv) {
		ts::ClassInfo                        base_class = ctv.type.getType();
		const std::vector<ts::AncestorData>& ancestors  = base_class.allAncestors();

		fillVtablePtr(ctv, base_class);

		for (auto ancestor_data: ancestors) {
			// @TODO: Can be optimised -- we know the offsets at compile time here, no need to check
			// the vtable
			if (ancestor_data.last_virtual_ancestor.has_value()) {
				usize vtable_position
					= base_class.getVtablePositionOf(ancestor_data.last_virtual_ancestor.value());
				CTV ancestor_ctv = getVirtualSubCtv(
					ctv, vtable_position, ancestor_data.offset, ts::TypeDesc<>(ancestor_data.info)
				);
				fillVtablePtr(ancestor_ctv, base_class);
			} else {
				CTV ancestor_ctv = ctv.subCTV(
					ts::TypeDesc<>(ancestor_data.info),
					ancestor_data.offset,
					ancestor_data.info.getBaseSize()
				);
				fillVtablePtr(ancestor_ctv, base_class);
			}
		}
	}
}
