#include "abstract_type_impl.hpp"

#include "queries.hpp"

// @TODO: #2331 Remove these includes
#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/nested_import_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/queries/implicit_coercibility.hpp>
#include <helios/tsh/queries/types.hpp>

#include <query_framework/context/context.hpp>

#include <utility>

namespace compiler::tsh {
	/**
	 * @brief Creates a human-readable string representation of a vector of symbol types.
	 * @param types Vector of symbol types to stringify.
	 * @return A human-readable string representing a sequence of symbol types.
	 */
	std::string stringifyTypeVector(const std::vector<SymbolType<>>& types) {
		std::stringstream res;
		res << "(";
		if (!types.empty()) res << types[0].toString();
		for (const auto& type: types | std::views::drop(1)) res << ", " << type.toString();
		res << ")";

		return res.str();
	}

	bool UnitAbstractTypeImpl::isImplicitlyCoercible(const AbstractType target, query::Context&)
		const {
		// The unit type can be coerced to the meta type
		// because unit values can be interpreted as unit types.
		return target.getKind() == Kind::Meta;
	}

	CRef<TypeInterface> UnitAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	bool PointerAbstractTypeImpl::isImplicitlyCoercible(
		const AbstractType target, query::Context& ctx
	) const {
		// Explicit override without change in implementation to add comment.
		// Implicit coercions allow checking against null pointer.
		// We do not allow casting to another (raw) pointer type,
		// because we forbid implicit type (de)specification in this context.
		// We only allow dropping mutability.
		return target.getKind() == Kind::Bool
		    || (target.getKind() == Kind::Pointer
		        && ctx.query<QueryImplicitCoercibilityOnSymbolType>(
					{ pointee, PointerAbstractType(target).getPointee() }
				));
	}

	bool TupleAbstractTypeImpl::isImplicitlyCoercible(
		const AbstractType target, query::Context& ctx
	) const {
		// Implicit coercions are allowed to other tuples of the same size,
		// where each component can be coerced independently.
		//
		// Additionally, tuples can be coerced to Meta type if
		// all their components can be coerced to Meta type.

		if (target.getKind() == Kind::Tuple) {
			const TupleAbstractType target_tuple = target;

			const std::vector<SymbolType<>>& target_components = target_tuple.getComponents();
			if (target_components.size() != components.size()) return false;

			for (usize i = 0; i < components.size(); i++) {
				SymbolType component = components[i];
				if (const SymbolType target_component = target_components[i];
				    !ctx.query<QueryImplicitCoercibilityOnSymbolType>({
						component,
						target_component,
					}))
					return false;
			}

			return true;
		}

		if (target.getKind() == Kind::Meta) {
			// Tuples can be coerced to the Meta type if and only if their components
			// can all be coerced to Meta type. Note that the components can have additional
			// indirection and mutability specifiers (the component types are symbol
			// types), but that's OK, we need only to check the abstract types underneath.
			for (const auto& component: components)
				if (!ctx.query<QueryImplicitCoercibilityOnAbstractType>({
						component.getType(),
						target,  //< target is the Meta type.
					}))
					return false;
			return true;
		}

		return false;
	}

	TupleAbstractTypeImpl::TupleAbstractTypeImpl(std::vector<SymbolType<>> components):
		  components(std::move(components)) {
		representation = "Tuple" + stringifyTypeVector(this->components);
	}

	FunctionAbstractTypeImpl::FunctionAbstractTypeImpl(
		std::vector<SymbolType<>> parameter_types,
		const SymbolType<>        result_type,
		const bool                pure,
		const bool                free
	):
		  parameter_types(std::move(parameter_types)),
		  result_type(result_type),
		  pure(pure),
		  free(free) {
		representation = "Function " + stringifyTypeVector(this->parameter_types) + " -> ("
		               + result_type.toString() + ")";
	}

	bool FunctionAbstractTypeImpl::isImplicitlyCoercible(
		const AbstractType target, query::Context& context
	) const {
		// A function type is coercible to another function type if and only if
		// the return type is coercible to the other return type and
		// the other parameter types are coercible to the parameter types,
		// similar to the rules of function subtyping.
		//
		// Additionally, only a pure function can be coerced to a pure function,
		// and only a free function can be coerced to a free function.

		if (target.getKind() != Kind::Function) return false;
		const FunctionAbstractType target_function = target;
		if ((!pure && target_function.isPure()) || (!free && target_function.isFree())
		    || parameter_types.size() != target_function.getParameterTypes().size()) {
			return false;
		}

		for (usize i = 0; i < parameter_types.size(); i++)
			if (!context.query<QueryImplicitCoercibilityOnSymbolType>(
					{ target_function.getParameterTypes()[i], parameter_types[i] }
				))
				return false;
		return context.query<QueryImplicitCoercibilityOnSymbolType>(
			{ result_type, target_function.getResultType() }
		);
	}

	VariantAbstractTypeImpl::VariantAbstractTypeImpl(const std::vector<SymbolType<>>& variant_types):
		  underlying_types(variant_types) {
		representation = "Variant " + stringifyTypeVector(underlying_types);
	}

	bool VariantAbstractTypeImpl::hasNoOpDestructor() const {
		for (const auto& type: underlying_types)
			if (!type.hasNoOpDestructor()) return false;
		return true;
	}

	ClassAbstractTypeImpl::ClassAbstractTypeImpl(compiler::helios::SymID symbol): symbol(symbol) {
		representation = "Class " + name(symbol).str();
	}

	CRef<TypeInterface> ClassAbstractTypeImpl::getInterface(query::Context& ctx) const {
		return &ctx.query<QueryInterfaceOfClass>(this)->valueOrThrow();
	}

	CRef<TypeInterface> VoidAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> ByteAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> BoolAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> CharAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> IntegralAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> FloatAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> RawPointerAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> PointerAbstractTypeImpl::getInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> StringAbstractTypeImpl::getInterface(query::Context&) const {
		throw base::NotYetImplemented("String type interface not yet implemented");
	}

	CRef<TypeInterface> DynamicArrayAbstractTypeImpl::getInterface(query::Context&) const {
		throw base::NotYetImplemented("Dynamic array type interface not yet implemented");
	}

	CRef<TypeInterface> StaticArrayAbstractTypeImpl::getInterface(query::Context&) const {
		throw base::NotYetImplemented("Static array type interface not yet implemented");
	}

	CRef<TypeInterface> TupleAbstractTypeImpl::getInterface(query::Context& ctx) const {
		return &ctx.query<QueryInterfaceOfTuple>(this)->valueOrThrow();
	}

	CRef<TypeInterface> FunctionAbstractTypeImpl::getInterface(query::Context&) const {
		throw base::NotYetImplemented("Function type interface not yet implemented");
	}

	CRef<TypeInterface> VariantAbstractTypeImpl::getInterface(query::Context&) const {
		throw base::NotYetImplemented("Variant type interface not yet implemented");
	}

	CRef<TypeInterface> NamespaceAbstractTypeImpl::getInterface(query::Context&) const {
		CORE_PANIC("Namespace type interface does not exist (we can add it if we find a use case).");
	}

	CRef<TypeInterface> ModuleAbstractTypeImpl::getInterface(query::Context&) const {
		CORE_PANIC("Module type interface does not exist (we can add it if we find a use case).");
	}

	CRef<TypeInterface> MetaAbstractTypeImpl::getInterface(query::Context&) const {
		throw base::NotYetImplemented("Meta type interface not yet implemented");
	}

	CRef<TypeInterface> ImportAbstractTypeImpl::getInterface(query::Context&) const {
		CORE_PANIC("Import type interface does not exist (we can add it if we find a use case).");
	}

	CRef<TypeInterface> TypeTemplateAbstractTypeImpl::getInterface(query::Context&) const {
		throw base::NotYetImplemented("Type template interface not yet implemented");
	}

	AbstractType TypeTemplateAbstractTypeImpl::instantiate(
		query::Context& ctx, const SymbolType<>& element_type
	) const {
		variant_match(source) {
			variant_case(BuiltinKind, builtin) {
				switch (builtin) {
				case TypeTemplateAbstractType::BuiltinKind::List: {
					return ctx.query<tsh::QueryDynamicArrayType>({ element_type });
				}
				default: {
					throw base::NotYetImplemented(base::strConcat(
						"Instantiation of a builtin type template type: ", representation
					));
				}
				}
			}
			variant_default {
				throw base::NotYetImplemented(base::strConcat(
					"Instantiation of a non-builtin type template type: ", representation
				));
			}
		}
		CORE_UNREACHABLE();
	}

	base::Optional<ClassAbstractType> ClassAbstractTypeImpl::getBaseClassType(query::Context& ctx
	) const {
		auto& base = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)->valueOrThrow().base;
		if (base.has_value()) return { ClassAbstractType(base.value()) };
		return {};
	}

	std::vector<ClassAbstractType> ClassAbstractTypeImpl::getImplementedInterfaceTypes(
		query::Context& ctx
	) const {
		auto& implements
			= ctx.query<compiler::helios::QueryClassSymbolData>(symbol)->valueOrThrow().implements;
		return { implements.begin(), implements.end() };
	}

	std::vector<compiler::helios::SymID> ClassAbstractTypeImpl::getImplementedInterfaceSymbols(
		query::Context& ctx
	) const {
		auto& implements
			= ctx.query<compiler::helios::QueryClassSymbolData>(symbol)->valueOrThrow().implements;
		// @TODO: change cast type to InterfaceInfo when interface type is created.
		constexpr auto TRANSFORMER = [](const AbstractType& interface) {
			return ClassAbstractType(interface).getSymbol();
		};
		auto view = std::ranges::ref_view(implements) | std::views::transform(TRANSFORMER);
		return { view.begin(), view.end() };
	}

	bool ClassAbstractTypeImpl::isDefaultConstructible(query::Context& ctx) const {
		auto fields = getInterface(ctx)->getFieldsView();
		for (const auto& field: fields) {
			// @TODO: #2331 Move this logic out of TSH.
			auto field_pst = helios::maybeSymbolPst(field.getSymbol())
			                     .value()
			                     .unlock(ctx)
			                     .dynamicCast<pst::Field>()
			                     .value();
			// If the field has an initializing value, then it's always constructible.
			if (field_pst->getInit().has_value()) continue;
			// Otherwise it has to be default constructible.
			if (!field.getType(ctx).isDefaultConstructible(ctx)) return false;
		}
		return true;
	}

	bool ClassAbstractTypeImpl::isTriviallyZeroInitializable(query::Context& ctx) const {
		auto fields = getInterface(ctx)->getFieldsView();
		for (const auto& field: fields) {
			// @TODO: #2331 Move this logic out of TSH.
			auto field_pst = helios::maybeSymbolPst(field.getSymbol())
			                     .value()
			                     .unlock(ctx)
			                     .dynamicCast<pst::Field>()
			                     .value();
			// If any of the fields has an initial value than the class is not trivially zero
			// initializable.
			if (field_pst->getInit().has_value()) return false;
			// All fields have to be trivially zero initializable.
			if (!field.getType(ctx).isTriviallyZeroInitializable(ctx)) return false;
		}
		return true;
	}

	bool ClassAbstractTypeImpl::isCopyable(query::Context& ctx) const {
		auto fields = getInterface(ctx)->getFieldsView();
		// All component types have to be copyable.
		return std::ranges::all_of(fields, [&](const auto& field) {
			return field.getType(ctx).isCopyable(ctx);
		});
	}

	bool ClassAbstractTypeImpl::isTriviallyCopyable(query::Context& ctx) const {
		auto fields = getInterface(ctx)->getFieldsView();
		// All component types have to be trivially copyable.
		return std::ranges::all_of(fields, [&](const auto& field) {
			return field.getType(ctx).isTriviallyCopyable(ctx);
		});
	}

	bool StaticArrayAbstractTypeImpl::isImplicitlyCoercible(AbstractType target, query::Context&)
		const {
		// Static arrays are implicitly coercible to dynamic arrays storing the same type.
		if (target.getKind() == Kind::DynamicArray) {
			auto dynamic_array_type = DynamicArrayAbstractType(target);
			return dynamic_array_type.getElementType() == element_type;
		}

		return false;
	}

	bool StaticArrayAbstractTypeImpl::hasNoOpDestructor() const {
		// Static arrays have trivial destructors if the inner type has a noOpDestructor or they
		// are zero sized.
		return size == 0 || element_type.getType().hasNoOpDestructor();
	}

	bool StaticArrayAbstractTypeImpl::isDefaultConstructible(query::Context& ctx) const {
		return size == 0 || element_type.isDefaultConstructible(ctx);
	}

	bool StaticArrayAbstractTypeImpl::isTriviallyZeroInitializable(query::Context& ctx) const {
		return size == 0 || element_type.isTriviallyZeroInitializable(ctx);
	}

	bool StaticArrayAbstractTypeImpl::isCopyable(query::Context& ctx) const {
		return size == 0 || element_type.isCopyable(ctx);
	}

	bool StaticArrayAbstractTypeImpl::isTriviallyCopyable(query::Context& ctx) const {
		return size == 0 || element_type.isTriviallyCopyable(ctx);
	}

	bool StaticArrayAbstractTypeImpl::carriesInformation(query::Context& ctx) const {
		// Static Arrays don't carry information if they don't contain any elements or contain
		// types that don't carry information.
		return element_type.getType().carriesInformation(ctx) && size > 0;
	}

	bool TupleAbstractTypeImpl::hasNoOpDestructor() const {
		return std::ranges::all_of(components, [&](const auto& component) {
			return component.hasNoOpDestructor();
		});
	}

	bool TupleAbstractTypeImpl::isDefaultConstructible(query::Context& ctx) const {
		return std::ranges::all_of(components, [&](const auto& component) {
			return component.isDefaultConstructible(ctx);
		});
	}

	bool TupleAbstractTypeImpl::isTriviallyZeroInitializable(query::Context& ctx) const {
		return std::ranges::all_of(components, [&](const auto& component) {
			return component.isTriviallyZeroInitializable(ctx);
		});
	}

	bool TupleAbstractTypeImpl::isCopyable(query::Context& ctx) const {
		return std::ranges::all_of(components, [&](auto& component) {
			return component.isCopyable(ctx);
		});
	}

	bool TupleAbstractTypeImpl::isTriviallyCopyable(query::Context& ctx) const {
		return std::ranges::all_of(components, [&](auto& component) {
			return component.isTriviallyCopyable(ctx);
		});
	}

	bool VariantAbstractTypeImpl::isDefaultConstructible(query::Context&) const {
		// Variant must be explicitly initialized with one of it's alternatives.
		return false;
	}

	bool VariantAbstractTypeImpl::isTriviallyZeroInitializable(query::Context&) const {
		return false;
	}

	bool VariantAbstractTypeImpl::isCopyable(query::Context& ctx) const {
		// Variant is copyable if all of it's underlying types are copyable.
		return std::ranges::all_of(underlying_types, [&](const auto& type) {
			return type.isCopyable(ctx);
		});
	}

	bool VariantAbstractTypeImpl::isTriviallyCopyable(query::Context& ctx) const {
		// Variant is trivially copyable if all of it's underlying types are trivially copyable.
		return std::ranges::all_of(underlying_types, [&](const auto& type) {
			return type.isTriviallyCopyable(ctx);
		});
	}
}
