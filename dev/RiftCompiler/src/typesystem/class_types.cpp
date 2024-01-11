#include "class_types.hpp"
#include "internal/type_info_impl.hpp"
#include "type_info.hpp"

#include <utility>

namespace ts {

	bool MemberInfo::isOk() const { return start_offset.has_value(); }

	MemberInfo AncestorInfo::operator^(const MemberInfo& a) const {
		if (result_type == ResultType::NoResult) return { .result_type = ResultType::NoResult };
		if (a.result_type == ResultType::NoResult) return a;
		if (result_type == ResultType::Ambiguous) return { .result_type = ResultType::Ambiguous };
		if (a.result_type == ResultType::Ambiguous) return a;
		if (a.result_type == ResultType::Virtual) return a;
		MemberInfo result{ .last_virtual_ancestor = last_virtual_ancestor,
			               .desc                  = a.desc,
			               .start_offset          = a.start_offset.value() + start_offset.value(),
			               .end_offset            = a.end_offset.value() + start_offset.value(),
			               .result_type           = result_type };

		return result;
	}

	AncestorInfo AncestorInfo::operator^(const AncestorInfo& a) const {
		if (result_type == ResultType::NoResult) return *this;
		if (a.result_type == ResultType::NoResult) return a;
		if (result_type == ResultType::Ambiguous) return *this;
		if (a.result_type == ResultType::Ambiguous) return a;
		if (a.result_type == ResultType::Virtual) return a;
		AncestorInfo result = *this;
		result.start_offset.value() += a.start_offset.value();
		result.end_offset.value() = a.end_offset.value() + start_offset.value();

		return result;
	}

	AncestorInfo& AncestorInfo::operator^=(const AncestorInfo& a) {
		if (result_type == ResultType::NoResult) return *this;
		if (a.result_type == ResultType::NoResult) {
			*this = a;
			return *this;
		}
		if (result_type == ResultType::Ambiguous) return *this;
		if (a.result_type == ResultType::Ambiguous) {
			*this = a;
			return *this;
		}
		if (a.result_type == ResultType::Virtual) {
			*this = a;
			return *this;
		}
		end_offset.value() = a.end_offset.value() + start_offset.value();
		start_offset.value() += a.start_offset.value();


		return *this;
	}

	bool AncestorInfo::isOk() const { return start_offset.has_value(); }

	ClassInfo ClassInfo::create(
		base::StrId                                              name,
		std::vector<std::pair<TypeDesc<>, symtable::SymbolId>>&& member_types,
		std::vector<std::pair<ClassInfo, InheritanceTag>>&&      inheritance,
		usize                                                    virtualMethods
	) {
		auto ptr = base::make_unique<internal::ClassInfoImpl>(
			name, std::move(member_types), std::move(inheritance), virtualMethods
		);

		auto res = ClassInfo(ptr.get());

		internal::pushType(std::move(ptr));
		return res;
	}

	ClassInfo ClassInfo::create(
		base::StrId name, std::vector<std::pair<TypeDesc<>, symtable::SymbolId>>&& member_types
	) {
		return ts::ClassInfo::create(name, std::move(member_types), {}, 0);
	}

	base::StrId ClassInfo::getName() const { return ((Pimpl) pimpl)->getName(); }

	[[nodiscard]]
	const std::vector<AncestorData> ClassInfo::basicParents() const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_impl_ptr->basicParents();
	}

	const std::vector<AncestorData>& ClassInfo::allAncestors() const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_impl_ptr->allAncestors();
	}

	// const std::map<ClassInfo, usize> ClassInfo::virtualParents() const {
	// 	const auto class_impl_ptr = (internal::ClassInfoImpl*)pimpl;
	//	return class_impl_ptr->virtualParents();
	//}
	const std::vector<ClassInfo> ClassInfo::virtualAncestors() const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_impl_ptr->virtualAncestors();
	}

	const std::vector<MemberData> ClassInfo::members() const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_impl_ptr->members();
	}

	const std::vector<MemberData>& ClassInfo::allMembers() const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_impl_ptr->allMembers();
	}

	MemberInfo ClassInfo::getMemberInfo(symtable::SymbolId symbol) const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_impl_ptr->getMemberInfo(symbol);
	}

	MemberInfo ClassInfo::getMemberInfo(
		symtable::SymbolId symbol, const std::vector<ClassInfo>& hint
	) const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_impl_ptr->getMemberInfo(symbol, hint);
	}

	AncestorInfo ClassInfo::getAncestorInfo(ClassInfo ancestor_id) const {
		return getAncestorInfo(std::vector<ClassInfo>({ ancestor_id }));
	}

	AncestorInfo ClassInfo::getAncestorInfo(const std::vector<ClassInfo>& ancestor_ids) const {
		const auto class_impl_ptr = (internal::ClassInfoImpl*) pimpl;
		auto       result         = class_impl_ptr->getAncestorInfo(ancestor_ids);
		return result;
	}

	// Returns the offset of a single specific virtual ancestor
	usize ClassInfo::getVirtualAncestorOffset(ClassInfo ancestor_id) const {
		const auto class_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_ptr->getVirtualAncestorOffset(ancestor_id);
	}

	// Returns all the offsets of virtual members of the clueless parent, as told by the
	// knowledgeable kid. Since the virtual members are put at the end of the kid, they will all be
	// positive.
	std::vector<std::pair<ClassInfo, usize>>
		ClassInfo::getVirtualAncestorTable(ClassInfo ancestor_id) const {
		const auto kid = (internal::ClassInfoImpl*) pimpl;
		return kid->getVirtualAncestorTable(ancestor_id);
	}

	std::vector<std::pair<ClassInfo, usize>>
		ClassInfo::getVirtualAncestorTable(std::vector<ClassInfo> ancestor_ids) const {
		const auto kid = (internal::ClassInfoImpl*) pimpl;
		return kid->getVirtualAncestorTable(std::move(ancestor_ids));
	}

	usize ClassInfo::getVtablePtrOffset() const {
		const auto class_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_ptr->getVtablePtrOffset();
	}

	usize ClassInfo::getVtableSize() const {
		const auto class_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_ptr->getVtableSize();
	}

	usize ClassInfo::getVtablePositionOf(ts::ClassInfo ancestor) const {
		const auto class_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_ptr->getVtablePositionOf(ancestor);
	}

	usize ClassInfo::getBaseSize() const {
		const auto class_ptr = (internal::ClassInfoImpl*) pimpl;
		return class_ptr->getBaseSize();
	}

	VTableInfo VTableInfo::create(ClassInfo class_info) {
		auto ptr    = base::make_unique<Impl>(class_info);
		auto vtable = VTableInfo(ptr.get());
		internal::pushType(std::move(ptr));
		return vtable;
	}

	ClassInfo VTableInfo::getAssociatedClass() { return ((Pimpl) pimpl)->getAssociatedClass(); }

	usize VTableInfo::getMethodCount() { return ((Pimpl) pimpl)->getMethodCount(); }

	usize VTableInfo::getParentCount() { return ((Pimpl) pimpl)->getParentCount(); }
}
