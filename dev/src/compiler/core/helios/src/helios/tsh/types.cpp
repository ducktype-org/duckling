/**
 * @file types.cpp
 * @brief Implementation of the simpler kinds of types.
 *
 * This file is not included outside the Type System module and can thus have full knowledge of the
 * underlying implementation hierarchy.
 */

#include "types.hpp"

#include "abstract_type.hpp"
#include "expression_type.hpp"

#include <helios_private/tsh/abstract_type_impl.hpp>

#include <base/except/exceptions.hpp>

#include <concepts>
#include <sstream>

// NOLINTBEGIN: linter assumes it's a function like macro
/**
 * @brief Explicitly instantiate the `checkDynamicCast` template.
 * @param ClassName The class name from the `AbstractType` hierarchy.
 */
#define INSTANTIATE_CHECKED_CAST(ClassName) \
	template ClassName::CPimpl checkDynamicCast<ClassName>(AbstractType::CPimpl);

// NOLINTEND

namespace compiler::tsh {

#define toCPimpl(pimpl) (CPimpl(reinterpret_cast<const Impl*>(pimpl.get())))

	/******************\
	|    BASIC TYPES   |
	\******************/

	// All creation methods were moved to queries.cpp.

	/*******************\
	|   POINTER TYPES   |
	\*******************/

	Bits IntegralAbstractType::getSize() const { return toCPimpl(pimpl)->getSize(); }

	Bits FloatAbstractType::getSize() const { return toCPimpl(pimpl)->getSize(); }

	IntegralAbstractType::Signedness IntegralAbstractType::getSignedness() const {
		return toCPimpl(pimpl)->getSignedness();
	}

	bool RawPointerAbstractType::isMutable() const { return toCPimpl(pimpl)->isMutable(); }

	SymbolType<> PointerAbstractType::getPointee() const { return toCPimpl(pimpl)->getPointee(); }

	AbstractType PointerAbstractType::getUnderlyingType() const {
		return toCPimpl(pimpl)->getUnderlyingType();
	}

	SymbolType<> ManyPointerAbstractType::getPointee() const {
		return toCPimpl(pimpl)->getPointee();
	}

	AbstractType ManyPointerAbstractType::getUnderlyingType() const {
		return toCPimpl(pimpl)->getUnderlyingType();
	}

	SymbolType<> CPointerAbstractType::getPointee() const { return toCPimpl(pimpl)->getPointee(); }

	AbstractType CPointerAbstractType::getUnderlyingType() const {
		return toCPimpl(pimpl)->getUnderlyingType();
	}

	struct ReferenceConstructionRecord {
		AbstractType  underlying_type;
		ReferenceKind ref_kind;
		bool          leaking, nullable, unique;

		auto operator<=>(const ReferenceConstructionRecord&) const = default;
	};

	/*******************\
	|  COMPOSITE TYPES  |
	\*******************/

	const std::vector<SymbolType<>>& TupleAbstractType::getComponents() const {
		return toCPimpl(pimpl)->getComponents();
	}

	std::vector<AbstractType> TupleAbstractType::getComponentAbstractTypes() const {
		const std::vector<SymbolType<>>& components = getComponents();
		std::vector<AbstractType>        component_types;
		component_types.reserve(components.size());
		for (const auto& component: components) component_types.push_back(component.getType());
		return component_types;
	}

	struct FunctionConstructionRecord {
		std::vector<AbstractType> parameter_types;
		ExpressionType<>          result_type;
		bool                      pure, free;

		auto operator<=>(const FunctionConstructionRecord&) const = default;
	};

	const std::vector<SymbolType<>>& FunctionAbstractType::getParameterTypes() const {
		return toCPimpl(pimpl)->getParameterTypes();
	}

	SymbolType<> FunctionAbstractType::getResultType() const {
		return toCPimpl(pimpl)->getResult();
	}

	bool FunctionAbstractType::isPure() const { return toCPimpl(pimpl)->isPure(); }

	bool FunctionAbstractType::isFree() const { return toCPimpl(pimpl)->isFree(); }

	SymbolType<> DynamicArrayAbstractType::getElementType() const {
		return toCPimpl(pimpl)->getElementType();
	}

	SymbolType<> StaticArrayAbstractType::getElementType() const {
		return toCPimpl(pimpl)->getElementType();
	}

	TypeTemplateAbstractType::Source TypeTemplateAbstractType::getSource() const {
		return toCPimpl(pimpl)->getSource();
	}

	AbstractType TypeTemplateAbstractType::instantiate(
		query::Context& ctx, const SymbolType<>& element_type
	) const {
		return toCPimpl(pimpl)->instantiate(ctx, element_type);
	}

	usize StaticArrayAbstractType::getSize() const { return toCPimpl(pimpl)->getSize(); }

	/*****************\
	|  NOMINAL TYPES  |
	\*****************/

	const std::vector<SymbolType<>>& VariantAbstractType::getUnderlyingTypes() const {
		return toCPimpl(pimpl)->getUnderlyingTypes();
	}

	SymbolType<> VariantAbstractType::getMember(const usize index) const {
		return toCPimpl(pimpl)->getMember(index);
	}

	compiler::helios::SymID ClassAbstractType::getSymbol() const {
		return toCPimpl(pimpl)->getSymbol();
	}

	base::Optional<ClassAbstractType> ClassAbstractType::getBaseClassType(query::Context& ctx
	) const {
		return toCPimpl(pimpl)->getBaseClassType(ctx);
	}

	base::Optional<compiler::helios::SymID> ClassAbstractType::getBaseClassSymbol(query::Context& ctx
	) const {
		return toCPimpl(pimpl)->getBaseClassSymbol(ctx);
	}

	std::vector<ClassAbstractType> ClassAbstractType::getImplementedInterfaceTypes(query::Context& ctx
	) const {
		return toCPimpl(pimpl)->getImplementedInterfaceTypes(ctx);
	}

	std::vector<compiler::helios::SymID> ClassAbstractType::getImplementedInterfaceSymbols(
		query::Context& ctx
	) const {
		return toCPimpl(pimpl)->getImplementedInterfaceSymbols(ctx);
	}

	SymbolType<> ClassAbstractType::getMemberType(
		const compiler::helios::SymID sym, query::Context& ctx
	) const {
		return toCPimpl(pimpl)->getMemberType(sym, ctx);
	}

	template<std::derived_from<AbstractType> TYPE_AbstractType>
	typename TYPE_AbstractType::CPimpl checkDynamicCast(const AbstractType::CPimpl pimpl) {
		auto result = dynamic_cast<const typename TYPE_AbstractType::Impl*>(pimpl.get());
		if (result == nullptr) {
			CORE_PANIC(
				"Type cast between TypeAbstractType kinds failed. ",
				"A cast from ",
				base::enumToStr(pimpl->getKind()),
				" to ",
				base::enumToStr(TYPE_AbstractType::Impl::STATIC_KIND),
				" was attempted."
			);
		}
		return result;
	}

	INSTANTIATE_CHECKED_CAST(UnitAbstractType)
	INSTANTIATE_CHECKED_CAST(VoidAbstractType)
	INSTANTIATE_CHECKED_CAST(ByteAbstractType)
	INSTANTIATE_CHECKED_CAST(BoolAbstractType)
	INSTANTIATE_CHECKED_CAST(CharAbstractType)
	INSTANTIATE_CHECKED_CAST(IntegralAbstractType)
	INSTANTIATE_CHECKED_CAST(FloatAbstractType)
	INSTANTIATE_CHECKED_CAST(RawPointerAbstractType)
	INSTANTIATE_CHECKED_CAST(PointerAbstractType)
	INSTANTIATE_CHECKED_CAST(ManyPointerAbstractType)
	INSTANTIATE_CHECKED_CAST(CPointerAbstractType)
	INSTANTIATE_CHECKED_CAST(SliceAbstractType)
	INSTANTIATE_CHECKED_CAST(StringAbstractType)
	INSTANTIATE_CHECKED_CAST(TupleAbstractType)
	INSTANTIATE_CHECKED_CAST(FunctionAbstractType)
	INSTANTIATE_CHECKED_CAST(DynamicArrayAbstractType)
	INSTANTIATE_CHECKED_CAST(StaticArrayAbstractType)
	INSTANTIATE_CHECKED_CAST(VariantAbstractType)
	INSTANTIATE_CHECKED_CAST(ClassAbstractType)
	INSTANTIATE_CHECKED_CAST(NamespaceAbstractType)
	INSTANTIATE_CHECKED_CAST(ModuleAbstractType)
	INSTANTIATE_CHECKED_CAST(MetaAbstractType)
	INSTANTIATE_CHECKED_CAST(TypeTemplateAbstractType)
}
