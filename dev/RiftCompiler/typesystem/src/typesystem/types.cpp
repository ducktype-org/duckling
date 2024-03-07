/**
 * \file types.cpp
 * \brief Implementation of the simpler kinds of types.
 *
 * This file is not included outside the Type System module and can thus have full knowledge of the
 * underlying implementation hierarchy.
 */

#include "type_info.hpp"

#include <sstream>
#include <concepts>
#include <ranges>

#include "internal/type_info_impl.hpp"
#include "type_desc.hpp"
#include <base/exceptions.hpp>

// NOLINTBEGIN: linter assumes it's a function like macro
/**
 * \brief Explicitly instantiate the `checkDynamicCast` template.
 * \param ClassName The class name from the `TypeInfo` hierarchy.
 */
#define INSTANTIATE_CHECKED_CAST(ClassName) \
	template ClassName::CPimpl checkDynamicCast<ClassName>(TypeInfo::CPimpl);

// NOLINTEND

// NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)
// We have a lot of C-style pointer casts here by design.
// We could change them to dynamic_casts but that's less legible and slower.
// We are reasonably confident that the pointer casts will never result in a bad cast.

namespace ts {

	/******************\
	|    BASIC TYPES   |
	\******************/

	UnitInfo UnitInfo::create() {
		// TODO: do we want to store unit_info on the vector as well?
		static auto unit_impl = internal::UnitInfoImpl{};
		static auto unit_info = UnitInfo{ &unit_impl };

		return unit_info;
	}

	VoidInfo VoidInfo::create() {
		// TODO: do we want to store void_info on the vector as well?
		static auto void_impl = internal::VoidInfoImpl{};
		static auto void_info = VoidInfo{ &void_impl };

		return void_info;
	}

	ByteInfo ByteInfo::create() {
		static auto byteImpl = Impl{};
		return ByteInfo{ &byteImpl };
	}

	BoolInfo BoolInfo::create() {
		static auto boolImpl = Impl{};
		return BoolInfo{ &boolImpl };
	}

	CharInfo CharInfo::create() {
		static auto charImpl = Impl{};
		return CharInfo{ &charImpl };
	}

	IntegralInfo IntegralInfo::create(usize size, bool signedness) {
		static std::map<std::pair<usize, bool>, Impl> ints = [] {
			std::map<std::pair<usize, bool>, Impl> result = {
				{ { 8, true }, Impl{ 8, true } },     { { 8, false }, Impl{ 8, false } },
				{ { 16, true }, Impl{ 16, true } },   { { 16, false }, Impl{ 16, false } },
				{ { 32, true }, Impl{ 32, true } },   { { 32, false }, Impl{ 32, false } },
				{ { 64, true }, Impl{ 64, true } },   { { 64, false }, Impl{ 64, false } },
				{ { 128, true }, Impl{ 128, true } }, { { 128, false }, Impl{ 128, false } },
			};
			for (const auto& v: std::views::values(result)) pushType(base::make_unique<Impl>(v));
			return result;
		}();

		RIFT_ASSERT(ints.contains({ size, signedness }), "Incorrect simple int size");

		return IntegralInfo{ &ints.at({ size, signedness }) };
	}

	FloatInfo FloatInfo::create(const usize size) {
		static std::map<usize, Impl> floats = {
			{ 16, Impl{ 16 } },  // For certain GPU applications
			{ 32, Impl{ 32 } },  // Standard float
			{ 64, Impl{ 64 } },  // Double precision
			{ 80, Impl{ 80 } },  // Long double, covers sum of ranges of int64 and uint64 precisely
			{ 128, Impl{ 128 } },  // Quad precision
		};

		RIFT_ASSERT(floats.contains(size), "Incorrect simple float size");

		return FloatInfo{ &floats.at(size) };
	}

	/*******************\
	|   POINTER TYPES   |
	\*******************/

	RawPointerInfo RawPointerInfo::create() {
		static auto raw_pointer_impl = Impl{};
		static auto raw_pointer      = RawPointerInfo{ &raw_pointer_impl };

		return raw_pointer;
	}

	PointerInfo PointerInfo::create(const TypeDesc<>& underlying_type) {
		static base::Map<TypeDesc<>, PointerInfo> pointers;

		if (!pointers.contains(underlying_type)) {
			auto pointer = base::make_unique<Impl>(underlying_type);
			pointers.put(underlying_type, PointerInfo{ pointer.get() });
			pushType(std::move(pointer));
		}

		return pointers[underlying_type];
	}

	TypeDesc<> PointerInfo::getUnderlyingType() const {
		return ((CPimpl) pimpl)->getUnderlyingType();
	}

	struct ReferenceConstructionRecord {
		TypeInfo      underlying_type;
		ReferenceKind ref_kind;
		bool          leaking, nullable, unique;

		auto operator<=>(const ReferenceConstructionRecord&) const = default;
	};

	ReferenceInfo ReferenceInfo::create(
		const TypeInfo      underlying_type,
		const ReferenceKind ref_kind,
		const bool          leaking,
		const bool          nullable,
		const bool          unique
	) {
		static base::Map<ReferenceConstructionRecord, ReferenceInfo> references;

		const auto ref_record
			= ReferenceConstructionRecord{ underlying_type, ref_kind, leaking, nullable, unique };

		if (!references.contains(ref_record)) {
			auto reference
				= base::make_unique<Impl>(underlying_type, ref_kind, leaking, nullable, unique);
			references.put(ref_record, ReferenceInfo(reference.get()));
			pushType(std::move(reference));
		}

		return references[ref_record];
	}

	TypeInfo ReferenceInfo::getUnderlyingType() const {
		return ((CPimpl) pimpl)->getUnderlyingType();
	}

	ReferenceKind ReferenceInfo::getReferenceKind() const {
		return ((CPimpl) pimpl)->getReferenceKind();
	}

	bool ReferenceInfo::isLeaking() const { return ((CPimpl) pimpl)->isLeaking(); }

	bool ReferenceInfo::isNullable() const { return ((CPimpl) pimpl)->isNullable(); }

	bool ReferenceInfo::isUnique() const { return ((CPimpl) pimpl)->isUnique(); }

	/*******************\
	|  COMPOSITE TYPES  |
	\*******************/

	struct FunctionConstructionRecord {
		std::vector<TypeDesc<>> parameter_types;
		TypeDesc<>              result_type;
		bool                    pure, free;

		auto operator<=>(const FunctionConstructionRecord&) const = default;
	};

	FunctionInfo FunctionInfo::create(
		const std::vector<TypeDesc<>>& parameter_types,
		const TypeDesc<>               result_type,
		const bool                     pure,
		const bool                     free
	) {
		static base::Map<FunctionConstructionRecord, FunctionInfo> function_pointers;

		const auto fptr_record
			= FunctionConstructionRecord{ parameter_types, result_type, pure, free };

		if (!function_pointers.contains(fptr_record)) {
			auto fptr = base::make_unique<Impl>(parameter_types, result_type, pure);
			function_pointers.put(fptr_record, FunctionInfo(fptr.get()));
			pushType(std::move(fptr));
		}

		return function_pointers[fptr_record];
	}

	std::vector<TypeDesc<>> FunctionInfo::getParameterTypes() const {
		return ((CPimpl) pimpl)->getParameterList();
	}

	TypeDesc<> FunctionInfo::getResultType() const { return ((CPimpl) pimpl)->getResult(); }

	bool FunctionInfo::isPure() const { return ((CPimpl) pimpl)->isPure(); }

	bool FunctionInfo::isFree() const { return ((CPimpl) pimpl)->isFree(); }

	/*****************\
	|  NOMINAL TYPES  |
	\*****************/

	VariantInfo VariantInfo::create(const std::vector<TypeDesc<>>& variant_types) {
		static base::Map<std::vector<TypeDesc<>>, VariantInfo> variants;
		if (variants.contains(variant_types)) return variants[variant_types];

		auto ptr = base::make_unique<Impl>(variant_types);

		variants.put(variant_types, VariantInfo{ ptr.get() });

		pushType(std::move(ptr));

		return variants[variant_types];
	}

	const std::vector<TypeDesc<>>& VariantInfo::getUnderlyingTypes() const {
		return ((CPimpl) pimpl)->getUnderlyingTypes();
	}

	TypeDesc<> VariantInfo::getMember(const usize index) const {
		return ((CPimpl) pimpl)->getMember(index);
	}

	NamespaceInfo NamespaceInfo::create() {
		static auto namespace_impl = Impl{};
		static auto namespace_info = NamespaceInfo{ &namespace_impl };

		return namespace_info;
	}

	ModuleInfo ModuleInfo::create() {
		static auto module_impl = Impl{};
		static auto module_info = ModuleInfo{ &module_impl };

		return module_info;
	}

	MetaInfo MetaInfo::create() {
		static auto meta = Impl{};
		return MetaInfo{ &meta };
	}

	template<std::derived_from<TypeInfo> TYPE_INFO>
	typename TYPE_INFO::CPimpl checkDynamicCast(TypeInfo::CPimpl p) {
		auto result = dynamic_cast<typename TYPE_INFO::CPimpl>(p);
		if (result == nullptr) {
			std::stringstream ss;
			const Kind        originalKind = p->getKind();
			const Kind        targetKind   = TYPE_INFO::Impl::staticKind;
			ss << "Type cast between TypeInfo kinds failed. A cast from "
			   << kindToString(originalKind) << " to " << kindToString(targetKind)
			   << " was attempted.";
			throw base::LogicError{ ss.str() };
		}
		return result;
	}

	INSTANTIATE_CHECKED_CAST(UnitInfo)
	INSTANTIATE_CHECKED_CAST(VoidInfo)
	INSTANTIATE_CHECKED_CAST(ByteInfo)
	INSTANTIATE_CHECKED_CAST(BoolInfo)
	INSTANTIATE_CHECKED_CAST(CharInfo)
	INSTANTIATE_CHECKED_CAST(IntegralInfo)
	INSTANTIATE_CHECKED_CAST(FloatInfo)
	INSTANTIATE_CHECKED_CAST(RawPointerInfo)
	INSTANTIATE_CHECKED_CAST(ReferenceInfo)
	INSTANTIATE_CHECKED_CAST(PointerInfo)
	INSTANTIATE_CHECKED_CAST(FunctionInfo)
	INSTANTIATE_CHECKED_CAST(VariantInfo)
	INSTANTIATE_CHECKED_CAST(NamespaceInfo)
	INSTANTIATE_CHECKED_CAST(ModuleInfo)
	INSTANTIATE_CHECKED_CAST(ClassInfo)
	INSTANTIATE_CHECKED_CAST(VTableInfo)
	INSTANTIATE_CHECKED_CAST(MetaInfo)
}

// NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)
