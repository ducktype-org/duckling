#include "abstract_type_impl.hpp"

#include <query_framework/context.hpp>

#include <utility>

namespace tsh::internal {
	/**
	 * @brief Gets the global AbstractTypeImpl storage structure.
	 * @return The global AbstractTypeImpl storage structure.
	 */
	std::vector<Box<const AbstractTypeImpl>>& getTypes() {
		static std::vector<Box<const AbstractTypeImpl>> abstract_type_impl_storage{};
		return abstract_type_impl_storage;
	}

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

		if (target.getKind() != Kind::Tuple) return false;
		TupleAbstractType target_tuple = target;

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

	VariantAbstractTypeImpl::VariantAbstractTypeImpl(const std::vector<SymbolType<>>& variant_types
	):
		  underlying_types(variant_types) {
		representation = "Variant " + stringifyTypeVector(underlying_types);
	}

	ClassAbstractTypeImpl::ClassAbstractTypeImpl(compiler::helios::SymID symbol): symbol(symbol) {
		representation = "Class " + name(symbol).str();
	}

	const TypeInterface& ClassAbstractTypeImpl::getInterface(query::Context& ctx) const {
		return *ctx.query<QueryInterfaceOfClass>(this);
	}

	base::Optional<ClassAbstractType> ClassAbstractTypeImpl::getBaseClassType(query::Context& ctx
	) const {
		auto& base = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
		                 ->expect("Not handling ERRORS in TS yet")
		                 .base;
		if (base.has_value()) return { ClassAbstractType(base.value()) };
		return {};
	}

	std::vector<ClassAbstractType>
		ClassAbstractTypeImpl::getImplementedInterfaceTypes(query::Context& ctx) const {
		auto& implements = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
		                       ->expect("Not handling ERRORS in TS yet")
		                       .implements;
		return { implements.begin(), implements.end() };
	}

	std::vector<compiler::helios::SymID>
		ClassAbstractTypeImpl::getImplementedInterfaceSymbols(query::Context& ctx) const {
		auto& implements = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
		                       ->expect("Not handling ERRORS in TS yet")
		                       .implements;
		// @TODO: change cast type to InterfaceInfo when interface type is created.
		constexpr auto transformer = [](const AbstractType& interface) {
			return ClassAbstractType(interface).getSymbol();
		};
		auto view = std::ranges::ref_view(implements) | std::views::transform(transformer);
		return { view.begin(), view.end() };
	}
}
