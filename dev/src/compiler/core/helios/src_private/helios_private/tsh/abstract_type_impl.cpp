#include "abstract_type_impl.hpp"

#include "queries.hpp"

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/hout_creation/definition_generation/copy_constructors.hpp>

// @TODO: #2331 Remove these includes
#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/expr_holders.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/all_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/nested_import_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/symbols/symbol_abi.hpp>
#include <helios/tsh/queries/implicit_coercibility.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/definition_generation/length_methods.hpp>
#include <helios_private/hout_creation/definition_generation/to_string_methods.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <unordered_set>
#include <utility>

namespace compiler::tsh {
	/**
	 * @brief Gets the default interface for a type, i.e. the methods which should be defined for
	 * every type, such as `toString`.
	 * @param ctx Query context for generating symbols.
	 * @param type The type for which to get the default interface, needed e.g. for method types.
	 * @return The default interface for the given type.
	 */
	TypeInterface getDefaultTypeInterfaceForType(query::Context& ctx, const AbstractType type) {
		using helios::defgen::destructSymForType;
		using helios::defgen::generatedToStringSymForType;

		// The generated `toString` returns a `String`, which lives in `core.containers`. It only
		// exists when a standard library is available, so skip it otherwise (e.g. no-std builds) —
		// resolving its type would fail to find the `String` language primitive.
		const bool has_to_string = isStringTypePresent(ctx);

		// @TODO: #1956 Methods don't work for zero-sized types yet, due to taking ref to self
		if (not type.carriesInformation(ctx)) {
			if (type.getKind() == Kind::Unit && has_to_string) {
				// The unit type has a `toString` method, even though it doesn't carry information,
				// because it is a simple type and it's passed by value.
				return TypeInterface{ std::vector{ InterfaceElement{
					generatedToStringSymForType(ctx, type),
					type,
					0,
					InterfaceElement::InterfaceElementKind::Method,
					ClassMemberVisibility::Public,
					InterfaceElement::SpecialKind::ToString,
				} } };
			}
			return {};
		}

		std::vector<InterfaceElement> elements;

		// Every type has a `toString` method (when a standard library provides `String`).
		if (has_to_string) {
			elements.emplace_back(
				generatedToStringSymForType(ctx, type),
				type,
				0,
				InterfaceElement::InterfaceElementKind::Method,
				ClassMemberVisibility::Public,
				InterfaceElement::SpecialKind::ToString
			);
		}

		// Only classes have destructors (for now)
		if (type.getKind() == Kind::Class) {
			elements.emplace_back(
				destructSymForType(ctx, type),
				type,
				0,
				InterfaceElement::InterfaceElementKind::Method,
				ClassMemberVisibility::Public
			);
		}

		return TypeInterface(elements);
	}

	/**
	 * Internal query for caching type interfaces.
	 */
	DECLARE_QUERY(QueryTypeInterface, AbstractType, CRef<TypeInterface>, ({ .uses_qresult = false }));

	CRef<TypeInterface> AbstractTypeImpl::getInterface(query::Context& ctx) const {
		return ctx.query<QueryTypeInterface>(AbstractType(this));
	}

	bool AbstractTypeImpl::isSimple() const {
		using enum Kind;
		switch (this->getKind()) {
		case Unit:
		case Void:
		case Bool:
		case Char:
		case Byte:
		case Integral:
		case Float:
		case Pointer:
		case ManyPointer:
		case CPointer:
		case RawPointer:
		case Slice:
			return true;
		default:
			return false;
		}
	}

	struct IMPLEMENT_QUERY(QueryTypeInterface, TypeInterface) {
		/**
		 * Combines the default interface with the declared one (by the user).
		 * It doesn't add the elements with special kind already existing.
		 */
		static TypeInterface addDefaultInterface(
			Context& ctx, CRef<TypeInterface> declared, const QKey key
		) {
			std::vector<InterfaceElement>                     new_elements;
			std::unordered_set<InterfaceElement::SpecialKind> declared_specials;
			for (auto& elem: declared->getElements()) {
				new_elements.push_back(elem);
				if (elem.specialKind() != InterfaceElement::SpecialKind::None)
					declared_specials.insert(elem.specialKind());
			}

			const TypeInterface default_interface = getDefaultTypeInterfaceForType(ctx, key);
			for (auto& elem: default_interface.getElements()) {
				if (elem.specialKind() != InterfaceElement::SpecialKind::None
				    && declared_specials.contains(elem.specialKind()))
					continue;
				new_elements.push_back(elem);
			}
			return TypeInterface(new_elements);
		}

		static auto provide(Context& ctx, const QKey key) -> PResult {
			return addDefaultInterface(ctx, key.getPimpl()->getDeclaredInterface(ctx), key);
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeInterface)

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

	CRef<TypeInterface> UnitAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	bool PointerAbstractTypeImpl::isImplicitlyCoercible(const AbstractType, query::Context&) const {
		// The pointers are generally not the main tool for the job
		// in our language, but we may in the future allow
		// implicit coercions to bool to check against null pointer.
		return false;
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
		// Variants are unordered; canonicalize the alternative order so that the runtime tag
		// (= index into getUnderlyingTypes()) does not depend on construction order.
		// Sorting must not use queryUnstablePerfectHash: it differs between compiler
		// processes, which would make the emitted code non-deterministic.
		std::ranges::stable_sort(underlying_types, [](const SymbolType<>& a, const SymbolType<>& b) {
			return a.toString() < b.toString();
		});
		representation = "Variant " + stringifyTypeVector(underlying_types);
	}

	bool VariantAbstractTypeImpl::isTriviallyDestructible(query::Context& ctx) const {
		for (const auto& type: underlying_types)
			if (!type.isTriviallyDestructible(ctx)) return false;
		return true;
	}

	ClassAbstractTypeImpl::ClassAbstractTypeImpl(compiler::helios::SymID symbol): symbol(symbol) {
		representation = "Class " + name(symbol).str();
	}

	CRef<TypeInterface> ClassAbstractTypeImpl::getDeclaredInterface(query::Context& ctx) const {
		return &ctx.query<QueryInterfaceOfClass>(this)->valueOrThrow();
	}

	CRef<TypeInterface> VoidAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> ByteAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> BoolAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> CharAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> IntegralAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> FloatAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> RawPointerAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> PointerAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> ManyPointerAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> CPointerAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		// note: we can extend interface later if needed
		static TypeInterface empty{};
		return &empty;
	}

	CRef<TypeInterface> SliceAbstractTypeImpl::getDeclaredInterface(query::Context& ctx) const {
		const auto type = toAbstractType().as<SliceAbstractType>();
		return &ctx.query<QueryInterfaceOfSlice>(type)->valueOrThrow();
	}

	CRef<TypeInterface> StaticArrayAbstractTypeImpl::getDeclaredInterface(query::Context& ctx
	) const {
		const auto type = toAbstractType().as<StaticArrayAbstractType>();
		return &ctx.query<QueryInterfaceOfStaticArray>(type)->valueOrThrow();
	}

	CRef<TypeInterface> TupleAbstractTypeImpl::getDeclaredInterface(query::Context& ctx) const {
		return &ctx.query<QueryInterfaceOfTuple>(this)->valueOrThrow();
	}

	CRef<TypeInterface> FunctionAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		throw base::NotYetImplemented("Function type interface not yet implemented");
	}

	CRef<TypeInterface> VariantAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		throw base::NotYetImplemented("Variant type interface not yet implemented");
	}

	CRef<TypeInterface> NamespaceAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		CORE_PANIC("Namespace type interface does not exist (we can add it if we find a use case).");
	}

	CRef<TypeInterface> ModuleAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		CORE_PANIC("Module type interface does not exist (we can add it if we find a use case).");
	}

	CRef<TypeInterface> MetaAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		CORE_PANIC("Meta type interface does not exist yet.");
	}

	CRef<TypeInterface> ImportAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		CORE_PANIC("Import type interface does not exist (we can add it if we find a use case).");
	}

	CRef<TypeInterface> TypeTemplateAbstractTypeImpl::getDeclaredInterface(query::Context&) const {
		throw base::NotYetImplemented("Type template interface not yet implemented");
	}

	base::Optional<ClassAbstractType> ClassAbstractTypeImpl::getBaseClassType(query::Context& ctx
	) const {
		auto& base = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)->valueOrThrow().base;
		if (base.has_value()) return { ClassAbstractType(base.value()) };
		return {};
	}

	compiler::helios::SymbolABI ClassAbstractTypeImpl::getABI(query::Context& ctx) const {
		return ctx.query<compiler::helios::QuerySymbolABI>(symbol)->valueOrThrow();
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
		auto fields = getDeclaredInterface(ctx)->getFieldsView();
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
		auto fields = getDeclaredInterface(ctx)->getFieldsView();
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
		// A user-defined copy constructor makes the class copyable regardless of its fields.
		if (compiler::helios::defgen::userCopyConstructorOf(ctx, symbol).has_value()) return true;

		auto fields = getDeclaredInterface(ctx)->getFieldsView();
		// All component types have to be copyable.
		return std::ranges::all_of(fields, [&](const auto& field) {
			return field.getType(ctx).isCopyable(ctx);
		});
	}

	bool ClassAbstractTypeImpl::isTriviallyCopyable(query::Context& ctx) const {
		// A user-defined copy constructor means copies must run user code, so the class is never
		// trivially copyable.
		if (compiler::helios::defgen::userCopyConstructorOf(ctx, symbol).has_value()) return false;

		auto fields = getDeclaredInterface(ctx)->getFieldsView();
		// All component types have to be trivially copyable.
		return std::ranges::all_of(fields, [&](const auto& field) {
			return field.getType(ctx).isTriviallyCopyable(ctx);
		});
	}

	bool ClassAbstractTypeImpl::isTriviallyDestructible(query::Context& ctx) const {
		// A user-defined destructor code, means the class is not trivially destructible.
		if (ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
		        ->valueOrThrow()
		        .destructor.has_value())
			return false;

		auto fields = getDeclaredInterface(ctx)->getFieldsView();
		// Otherwise the destructor is a no-op only if every field is trivially destructible.
		return std::ranges::all_of(fields, [&](const auto& field) {
			return field.getType(ctx).isTriviallyDestructible(ctx);
		});
	}

	bool StaticArrayAbstractTypeImpl::isImplicitlyCoercible(AbstractType, query::Context&) const {
		return false;
	}

	bool SliceAbstractTypeImpl::isImplicitlyCoercible(AbstractType, query::Context&) const {
		return false;
	}

	bool StaticArrayAbstractTypeImpl::isTriviallyDestructible(query::Context& ctx) const {
		// Static arrays have trivial destructors if the inner type is trivially destructible or
		// they are zero sized.
		return size == 0 || element_type.getType().isTriviallyDestructible(ctx);
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

	bool TupleAbstractTypeImpl::isTriviallyDestructible(query::Context& ctx) const {
		return std::ranges::all_of(components, [&](const auto& component) {
			return component.isTriviallyDestructible(ctx);
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
