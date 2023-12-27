/**
 * @file class_types.hpp
 * @brief ClassInfo implementation
 */

#pragma once

#include "type_desc.hpp"
#include "type_desc.tcpp"
#include "type_info.hpp"

#include <hir/symtable/scope_symbol_id.hpp>
#include <utility>
#include <base/optional.hpp>
#include <base/exceptions.hpp>

namespace ts {

	class C3LinearisationException: public base::Exception {
		[[nodiscard]]
		const char* what() const noexcept override {
			return "C3 linearisation impossible for this class";
		}
	};

	struct InheritanceTag {
		enum class Kind {
			Public,
			Protected,
			Private,
		} kind;

		bool is_virtual;

		InheritanceTag(bool is_virtual, ts::InheritanceTag::Kind kind):
			  kind(kind),
			  is_virtual(is_virtual) {}
	};

	struct MemberInfo;
	struct AncestorInfo;

	struct MemberData;
	struct AncestorData;

	class ClassInfo: public TypeInfo {
		SETUP_TYPE(ClassInfo, TypeInfo)

	public:
		// These function will take more params in the future
		// Constructors for classes
		static ClassInfo create(
			base::StrId name, std::vector<std::pair<TypeDesc<>, symtable::SymbolId>>&& member_types
		);
		static ClassInfo create(
			base::StrId                                              name,
			std::vector<std::pair<TypeDesc<>, symtable::SymbolId>>&& member_types,
			std::vector<std::pair<ClassInfo, InheritanceTag>>&&      inheritance,
			usize                                                    virtualMethods
		);

		[[nodiscard]]
		base::StrId getName() const;

		[[nodiscard]]
		const std::vector<AncestorData>& allAncestors() const;

		// All the non-virtual parents
		[[nodiscard]]
		const std::vector<AncestorData> basicParents() const;

		// All the virtual parents, temporarily commented out to make sure it's not used
		// accidentally
		// const std::map<ClassInfo, usize> virtualParents() const;

		// All the virtual ancestors
		[[nodiscard]]
		const std::vector<ClassInfo> virtualAncestors() const;

		// All of our direct members
		[[nodiscard]]
		const std::vector<MemberData> members() const;

		// All of our members recursively
		[[nodiscard]]
		const std::vector<MemberData>& allMembers() const;

		// Gets all the info we could possibly want about our member
		// If we give a vector of ancestors, they will work as an inheritance path hint
		[[nodiscard]]
		MemberInfo getMemberInfo(symtable::SymbolId symbol) const;
		[[nodiscard]]
		MemberInfo
			getMemberInfo(symtable::SymbolId symbol, const std::vector<ClassInfo>& hint) const;

		// Gets all the info we could possibly want about our ancestor
		// If we give a whole vector of ancestors, they will work as an inheritance path hint
		[[nodiscard]]
		AncestorInfo getAncestorInfo(ClassInfo ancestor_id) const;
		[[nodiscard]]
		AncestorInfo getAncestorInfo(std::vector<ClassInfo> ancestor_ids) const;

		// Returns the offset of a single specific virtual ancestor
		[[nodiscard]]
		usize getVirtualAncestorOffset(ClassInfo ancestor_id) const;

		// Returns all the offsets of virtual members of the ancestor (its vtable), assuming it's
		// been created as a part of us.
		[[nodiscard]]
		std::vector<std::pair<ClassInfo, usize>> getVirtualAncestorTable(ClassInfo ancestor_id
		) const;
		[[nodiscard]]
		std::vector<std::pair<ClassInfo, usize>>
			getVirtualAncestorTable(std::vector<ClassInfo> ancestor_ids) const;

		// Gets the position of the vtable in this class' memory
		[[nodiscard]]
		usize getVtablePtrOffset() const;
		// Gets size of the vtable
		[[nodiscard]]
		usize getVtableSize() const;

		[[nodiscard]]
		usize getVtablePositionOf(ClassInfo ancestor) const;

		[[nodiscard]]
		usize getBaseSize() const;

		CHECKED_CAST(ClassInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(ClassInfo)
	};


	enum class ResultType { NoResult, Standard, Virtual, Ambiguous };

	struct MemberInfo {
		// If there is a virtual in the inheritance path, this is the last one. Otherwise, it's
		// an empty optional.
		base::Optional<ClassInfo> last_virtual_ancestor{};

		base::Optional<TypeDesc<>> desc{};

		// If the member is in a virtual ancestor, this data is relative to the ancestor's start
		// The exact offset of course needs to be looked up in the vtable at runtime
		base::Optional<usize> start_offset{};
		base::Optional<usize> end_offset{};

		// If the field below is not equal to Standard or Virtual, the data structure will be
		// full of empty optionals.
		ResultType result_type{ ResultType::NoResult };

		// Returns whether the result is correct, i.e. Standard or Virtual
		[[nodiscard]]
		bool isOk() const;
	};

	struct AncestorInfo {
		// If there is a virtual in the inheritance path, this is the last one. Otherwise, it's
		// an empty optional.
		base::Optional<ClassInfo> last_virtual_ancestor{};

		// If the member is in a virtual ancestor, this data is relative to the ancestor's start
		// The exact offset of course needs to be looked up in the vtable at runtime
		base::Optional<usize> start_offset{};
		base::Optional<usize> end_offset{};

		// If the field below is not equal to Standard or Virtual, this data structure will be
		// full of empty optionals.
		ResultType result_type{ ResultType::NoResult };

		// The ^ operators combine results
		// For example, if R1 tells us B has offset 10 in A, and R2 tells us C has offset 10 in B,
		// then R1^R2 will let us know that C has offset 20 in A.
		AncestorInfo  operator^(const AncestorInfo& a) const;
		AncestorInfo& operator^=(const AncestorInfo& a);

		MemberInfo operator^(const MemberInfo& a) const;

		// Returns whether the result is correct, i.e. Standard or Virtual
		[[nodiscard]]
		bool isOk() const;
	};

	// @TODO: In the future, Info and Data should be merged into one struct
	// Exceptions should handle bad results
	// However, what worries me, is that then people may use the result without checking if it's
	// virtual
	struct MemberData {
		symtable::SymbolId symbol;
		TypeDesc<>         desc;
		usize              offset;

		base::Optional<ClassInfo> last_virtual_ancestor{};

		// MemberData(symtable::SymbolId symbol, TypeDesc<> desc, usize offset): symbol(symbol),
		// desc(desc), offset(offset) {}
	};

	struct AncestorData {
		ClassInfo info;
		usize     offset;

		base::Optional<ClassInfo> last_virtual_ancestor{};

		// AncestorData(ClassInfo class_info, usize offset): info(class_info), offset(offset) {}
	};

	class VTableInfo: public TypeInfo {
		SETUP_TYPE(VTableInfo, TypeInfo)

	public:
		static VTableInfo create(ClassInfo class_info);
		ClassInfo         getAssociatedClass();
		usize             getParentCount();
		usize             getMethodCount();

		CHECKED_CAST(VTableInfo)

	protected:
		CONSTRUCT_FROM_IMPLEMENTATION(VTableInfo)
	};

}
