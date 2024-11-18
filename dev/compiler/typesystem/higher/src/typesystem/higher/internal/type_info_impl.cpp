#include "type_info_impl.hpp"
#include <query_framework/query_impl.hpp>
#include <utility>

namespace tsh::internal {
	/**
	 * @brief Gets the global TypeInfoImpl storage structure.
	 * @return The global TypeInfoImpl storage structure.
	 */
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes() {
		static std::vector<base::unique_ptr<const TypeInfoImpl>> type_info_impl_storage{};
		return type_info_impl_storage;
	}

	/**
	 * @brief Creates a human-readable string representation of a vector of component types.
	 * @param types Vector of component types to stringify.
	 * @return A human-readable string representing a sequence of component types.
	 */
	std::string stringifyTypeVector(const std::vector<ComponentType>& types) {
		std::stringstream res;
		res << "(";
		if (!types.empty()) res << (types[0].is_mutable ? "mut " : "") << types[0].type.toString();
		for (int i = 1; i < types.size(); i++)
			res << ", " << (types[i].is_mutable ? "mut " : "") << types[i].type.toString();
		res << ")";

		return res.str();
	}

	/**
	 * @brief Creates a human-readable string representation of a vector of types.
	 * @param types Vector of types to stringify.
	 * @return A human-readable string representing a sequence of types.
	 */
	std::string stringifyTypeVector(const std::vector<TypeInfo>& types) {
		std::vector<ComponentType> immutableTypes;
		immutableTypes.reserve(types.size());
		for (const auto& t: types) immutableTypes.emplace_back(t, false);
		return stringifyTypeVector(immutableTypes);
	}

	TupleInfoImpl::TupleInfoImpl(std::vector<ComponentType> components):
		  components(std::move(components)) {
		representation = stringifyTypeVector(this->components);
	}

	FunctionInfoImpl::FunctionInfoImpl(
		std::vector<TypeInfo> parameter_types,
		const TypeInfo        result_type,
		const bool            pure,
		const bool            free
	):
		  parameter_types(std::move(parameter_types)),
		  result_type(result_type),
		  pure(pure),
		  free(free) {
		representation = "Function " + stringifyTypeVector(this->parameter_types) + " -> ("
		               + result_type.toString() + ")";
	}

	bool FunctionInfoImpl::isImplicitlyCoercible(const TypeInfo target, query::Context& context)
		const {
		// A function type is coercible to another function type if and only if
		// the return type is coercible to the other return type and
		// the other parameter types are coercible to the parameter types,
		// similar to the rules of function subtyping.
		//
		// Additionally, only a pure function can be coerced to a pure function,
		// and only a free function can be coerced to a free function.

		if (target.getKind() != Kind::Function) return false;
		const FunctionInfo targetFunction = target;
		if ((!pure && targetFunction.isPure()) || (!free && targetFunction.isFree())
		    || parameter_types.size() != targetFunction.getParameterTypes().size()) {
			return false;
		}

		for (usize i = 0; i < parameter_types.size(); i++)
			if (!context.query<QueryImplicitCoercibilityOnInfo>({
					targetFunction.getParameterTypes()[i],
					parameter_types[i],
				}))
				return false;
		return context.query<QueryImplicitCoercibilityOnInfo>({
			result_type,
			targetFunction.getResultType(),
		});
	}

	VariantInfoImpl::VariantInfoImpl(const std::vector<TypeInfo>& variant_types):
		  underlying_types(variant_types) {
		representation = "Variant " + stringifyTypeVector(variant_types);
	}

	ClassInfoImpl::ClassInfoImpl(compiler::helios::SymID symbol): symbol(symbol) {
		representation = "Class " + name(symbol).str();
	}

	const TypeInterface& ClassInfoImpl::getInterface(query::Context& ctx) const {
		return *ctx.query<QueryInterfaceOfClass>(this);
	}

	base::Optional<ClassInfo> ClassInfoImpl::getBaseClassType(query::Context& ctx) const {
		auto& base = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
		                 ->expect("Not handling ERRORS in TS yet")
		                 .base;
		if (base.has_value()) return { ClassInfo(base.value()) };
		return {};
	}

	std::vector<ClassInfo> ClassInfoImpl::getImplementedInterfaceTypes(query::Context& ctx) const {
		auto& implements = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
		                       ->expect("Not handling ERRORS in TS yet")
		                       .implements;
		return { implements.begin(), implements.end() };
	}

	std::vector<compiler::helios::SymID>
		ClassInfoImpl::getImplementedInterfaceSymbols(query::Context& ctx) const {
		auto& implements = ctx.query<compiler::helios::QueryClassSymbolData>(symbol)
		                       ->expect("Not handling ERRORS in TS yet")
		                       .implements;
		// @TODO: change cast type to InterfaceInfo when interface type is created.
		constexpr auto transformer
			= [](const tsh::TypeInfo& interface) { return ClassInfo(interface).getSymbol(); };
		auto view = std::ranges::ref_view(implements) | std::views::transform(transformer);
		return { view.begin(), view.end() };
	}
}
