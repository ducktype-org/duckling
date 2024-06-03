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

namespace ts {

#define toCPimpl(pimpl) (reinterpret_cast<CPimpl>(pimpl))

	/******************\
	|    BASIC TYPES   |
	\******************/

	// All creation methods were moved to queries.cpp.

	/*******************\
	|   POINTER TYPES   |
	\*******************/

	std::string ComponentType::toString() const {
		return (is_mutable ? "mut " : "") + type.toString();
	}

	bool ComponentType::isImplicitlyCoercible(
		const ComponentType target, query::detail::ContextType& ctx
	) const {
		return type.isImplicitlyCoercible(target.type, ctx) && (is_mutable || !target.is_mutable);
	}

	std::strong_ordering ComponentType::operator<=>(const ComponentType& other) const = default;

	base::HashT ComponentType::customPerfectHash() const {
		return reinterpret_cast<std::size_t>(type.getPimpl()) + is_mutable;
	}

	bool RawPointerInfo::isMutable() const { return toCPimpl(pimpl)->isMutable(); }

	ComponentType PointerInfo::getComponent() const { return toCPimpl(pimpl)->getComponent(); }

	TypeInfo PointerInfo::getUnderlyingType() const { return toCPimpl(pimpl)->getUnderlyingType(); }

	bool PointerInfo::isMutable() const { return toCPimpl(pimpl)->isMutable(); }

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
		return toCPimpl(pimpl)->getUnderlyingType();
	}

	ReferenceKind ReferenceInfo::getReferenceKind() const {
		return toCPimpl(pimpl)->getReferenceKind();
	}

	bool ReferenceInfo::isLeaking() const { return toCPimpl(pimpl)->isLeaking(); }

	bool ReferenceInfo::isNullable() const { return toCPimpl(pimpl)->isNullable(); }

	bool ReferenceInfo::isUnique() const { return toCPimpl(pimpl)->isUnique(); }

	/*******************\
	|  COMPOSITE TYPES  |
	\*******************/

	const std::vector<ComponentType>& TupleInfo::getComponents() const {
		return toCPimpl(pimpl)->getComponents();
	}

	struct FunctionConstructionRecord {
		std::vector<TypeInfo> parameter_types;
		TypeDesc<>            result_type;
		bool                  pure, free;

		auto operator<=>(const FunctionConstructionRecord&) const = default;
	};

	const std::vector<TypeInfo>& FunctionInfo::getParameterTypes() const {
		return toCPimpl(pimpl)->getParameterTypes();
	}

	TypeInfo FunctionInfo::getResultType() const { return toCPimpl(pimpl)->getResult(); }

	bool FunctionInfo::isPure() const { return toCPimpl(pimpl)->isPure(); }

	bool FunctionInfo::isFree() const { return toCPimpl(pimpl)->isFree(); }

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
		return toCPimpl(pimpl)->getUnderlyingTypes();
	}

	TypeDesc<> VariantInfo::getMember(const usize index) const {
		return toCPimpl(pimpl)->getMember(index);
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
	INSTANTIATE_CHECKED_CAST(TupleInfo)
	INSTANTIATE_CHECKED_CAST(FunctionInfo)
	INSTANTIATE_CHECKED_CAST(VariantInfo)
	INSTANTIATE_CHECKED_CAST(NamespaceInfo)
	INSTANTIATE_CHECKED_CAST(ModuleInfo)
	INSTANTIATE_CHECKED_CAST(ClassInfo)
	INSTANTIATE_CHECKED_CAST(VTableInfo)
	INSTANTIATE_CHECKED_CAST(MetaInfo)
}
