#include "exec_default.hpp"

namespace exec {
	namespace {
		using Calls = const operation::Calls&;
	}

	CTV defaultEquality(ts::TypeInfo type_info, Calls calls, const CTV& a, const CTV& b) {
		RIFT_ASSERT(
			a.type.getType() == type_info && b.type.getType() == type_info,
			"Default equality can only compare values of this same type."
		);

		bool res = true;
		for (const operation::Call& call: calls) {
			CTV tempA    = a.subCTV(call.type, call.offset, call.size);
			CTV tempB    = b.subCTV(call.type, call.offset, call.size);
			CTV localRes = call({ tempA, tempB });
			res &= localRes.getData<bool>().front();
			if (!res) break;
		}

		CTV ctvResult                     = alloc_new(query::queryEntryPoint<ts::QueryBoolType>({}), 8);
		ctvResult.getData<bool>().front() = res;
		return ctvResult;
	}

	CTV defaultEqualityVirtual(ts::ClassInfo class_info, const CTV& a, const CTV& b) {
		bool res = true;

		auto vtable = query::queryEntryPoint<ts::QueryPointerType>({ts::VTableInfo::create(class_info)});

		usize vtable_offset = class_info.getVtablePtrOffset();

		auto vtable_ptr_a = a.subCTV(vtable, vtable_offset, ts::POINTER_SIZE);


		auto vtable_a = vtable_ptr_a.getDataUnderPointer<usize>();

		auto vtable_ptr_b = b.subCTV(vtable, vtable_offset, ts::POINTER_SIZE);

		// w vtable trzymane są offsety rozmiaru usize	(oraz w przyszłości metody)
		auto vtable_b = vtable_ptr_b.getDataUnderPointer<usize>();

		for (auto ancestor_info: class_info.virtualAncestors()) {
			usize pos = class_info.getVtablePositionOf(ancestor_info);

			usize off_a = vtable_a[pos];
			usize off_b = vtable_b[pos];

			CTV sub_a = a.subCTV(ancestor_info, off_a, ancestor_info.getBaseSize());
			CTV sub_b = b.subCTV(ancestor_info, off_b, ancestor_info.getBaseSize());

			// W przyszłości tutaj powinniśmy wyszukiwać operację, która porównuje tylko
			// niewirtualnych przodków (będzie ona zapewne przechowywana gdzie indziej)
			// Możliwe też że użytkownik sam zdefiniował operację porównywania której szukamy
			// wówczas nie będzie ona miała takiego podziału.
			const auto& op = operation::getDefault(operation::Defaultable::Equality, ancestor_info);

			CTV localRes = op({ sub_a, sub_b });

			res &= localRes.getData<bool>().front();
			if (!res) break;
		}

		CTV ctvResult                     = alloc_new(query::queryEntryPoint<ts::QueryBoolType>({}), 8);
		ctvResult.getData<bool>().front() = res;
		return ctvResult;
	}

	CTV defaultCompare(ts::TypeInfo type_info, Calls calls, const CTV& a, const CTV& b) {
		RIFT_ASSERT(
			a.type.getType() == type_info && b.type.getType() == type_info,
			"Default compare can only compare values of this same type."
		);

		int8_t res = 0;
		for (const auto& call: calls) {
			CTV tempA    = a.subCTV(call.type, call.offset, call.size);
			CTV tempB    = b.subCTV(call.type, call.offset, call.size);
			CTV localRes = call({ tempA, tempB });
			res          = localRes.getData<int8_t>().front();
			if (res != 0) break;
		}

		CTV ctvResult = alloc_new(query::queryEntryPoint<ts::QueryIntegralType>({ 8 }), 8);
		ctvResult.getData<int8_t>().front() = res;
		return ctvResult;
	}

	CTV defaultAssign(ts::TypeInfo type_info, Calls calls, const CTV& a, const CTV& b) {
		RIFT_ASSERT(
			a.type.getType() == type_info && b.type.getType() == type_info,
			"Default assign can only assign values of this same type."
		);

		for (const auto& call: calls) {
			CTV tempA = a.subCTV(call.type, call.offset, call.size);
			CTV tempB = b.subCTV(call.type, call.offset, call.size);
			call({ tempA, tempB });
		}

		return a;
	}

	CTV defaultConstructEmpty(ts::TypeInfo type_info, Calls calls, const CTV& ctv) {
		RIFT_ASSERT(
			ctv.type.getType() == type_info,
			"CTV of invalid type given to default empty constructor."
		);

		for (const auto& call: calls) {
			CTV temp = ctv.subCTV(call.type, call.offset, call.size);
			call({ temp });
		}

		return ctv;
	}

	CTV defaultConstructFull(ts::TypeInfo type_info, Calls calls, const std::vector<CTV>& ctvs) {
		RIFT_ASSERT(
			ctvs[0].type.getType() == type_info,
			"Default full constructor received incorrect CTV to construct."
		);

		RIFT_ASSERT(
			ctvs.size() == calls.size() + 1,
			"Default full constructor should receive value for each call, and one target CTV."
		);

		// First ctv is the one we are constructing.
		// @TODO: think about better semantics of this operation (how to set values of
		// parents?)
		const CTV& ctv = ctvs[0];


		for (usize i = 0; i < calls.size(); i++) {
			const auto& call = calls[i];
			const CTV&  val  = ctvs[i + 1];
			CTV         temp = ctv.subCTV(call.type, call.offset, call.size);
			call({ temp, val });
		}

		return ctv;
	}
}
