#include "vtable_creation.hpp"

namespace exec {
	CTV getVtable(ts::ClassInfo parent, ts::ClassInfo child) {

		if (internal::vtables.contains({parent, child})) {
			return internal::vtables.at({parent, child});
		}

		auto table = child.getVirtualAncestorTable(parent);

		auto ctv = alloc_new(ts::IntegralInfo::create(64), parent.getVtableSize()*sizeof(size_t)*8);
		//							^^^ TO UWZGLĘDNIA METODY WIRTUALNE

		for(size_t i = 0; i < table.size(); i++) {
			ctv.getData<size_t>()[i] = table[i].second;
		}

		internal::vtables.insert({{parent, child}, ctv});

		return ctv;
	}

	CTV getVirtualSubCtv(CTV ctv, size_t vtable_position, size_t offset, ts::TypeDesc<> wanted_type) {
		ts::ClassInfo baseClass = ctv.type.getType();
		size_t vtable_ptr_offset = baseClass.getVtablePtrOffset();

		auto vtable_ptr_ctv = ctv.subCTV(ts::PointerInfo::create(ts::VTableInfo::create(baseClass)), vtable_ptr_offset, ts::POINTER_SIZE);

		auto ancestor_offset = vtable_ptr_ctv.getDataUnderPointer<size_t>()[vtable_position];

		return ctv.subCTV(wanted_type, ancestor_offset + offset, wanted_type.getType().getSize());
	}

	void fillVtablePtr(CTV ctv, ts::ClassInfo base_class) {
		ts::ClassInfo our_info = ctv.type.getType();
		if (our_info.getVtableSize() > 0) {
			size_t vtable_ptr_offset = our_info.getVtablePtrOffset();
			auto vtable_ptr_ctv = ctv.subCTV(ts::RawPointerInfo::create(), vtable_ptr_offset, ts::POINTER_SIZE);

			// @TODO: Make this a normal memcopy
			vtable_ptr_ctv.getData<size_t>().front() = getVtable(our_info, base_class).makePointer().getData<size_t>().front();
		}
	}

	void fillAllVtablePtrs(CTV ctv) {
		ts::ClassInfo base_class = ctv.type.getType();
		const std::vector<ts::AncestorData> &ancestors = base_class.allAncestors();

		fillVtablePtr(ctv, base_class);

		for (auto ancestor_data : ancestors) {
			// @TODO: Can be optimised -- we know the offsets at compile time here, no need to check the vtable
			if (ancestor_data.last_virtual_ancestor.has_value()) {
				size_t vtable_position = base_class.getVtablePositionOf(ancestor_data.last_virtual_ancestor.value());
				CTV ancestor_ctv = getVirtualSubCtv(ctv, vtable_position, ancestor_data.offset, ts::TypeDesc<>(ancestor_data.info));
				fillVtablePtr(ancestor_ctv, base_class);
			}
			else {
				CTV ancestor_ctv = ctv.subCTV(ts::TypeDesc<>(ancestor_data.info), ancestor_data.offset, ancestor_data.info.getBaseSize());
				fillVtablePtr(ancestor_ctv, base_class);
			}
		}
	}
}
