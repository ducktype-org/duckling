#include "type_info_impl.hpp"
#include <queue>
#include <utility>

namespace ts::internal {
	/**
	 * @brief Gets the global TypeInfoImpl storage structure.
	 * @return The global TypeInfoImpl storage structure.
	 */
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes() {
		static std::vector<base::unique_ptr<const TypeInfoImpl>> type_info_impl_storage{};
		return type_info_impl_storage;
	}

	/**
	 * @brief Creates a human-readable string representation of a vector of types.
	 * @param types Vector of types to stringify.
	 * @return A human-readable string representing a sequence of types.
	 * @todo Remove when all types stop using TypeDesc for member types.
	 */
	std::string stringifyTypeVector(const std::vector<TypeDesc<>>& types) {
		std::string res = "(";
		for (const auto& t: types) res += t.getType().toString() + ",";
		res += ")";

		return res;
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
		representation = stringifyTypeVector(components);
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

	VariantInfoImpl::VariantInfoImpl(const std::vector<TypeInfo>& variant_types):
		  underlying_types(variant_types) {
		representation = "Variant " + stringifyTypeVector(variant_types);
	}

	ClassInfoImpl::ClassInfoImpl(compiler::helios::SymID symbol): symbol(symbol) {
		representation = "Class " + name(symbol).str();
	}

	/*[[nodiscard]]
	ClassInfo VTableInfoImpl::getAssociatedClass() const {
		return associated_class;
	}

	[[nodiscard]]
	usize VTableInfoImpl::getParentCount() const {
		return associated_class.virtualAncestors().size();
	}

	[[nodiscard]]
	usize VTableInfoImpl::getMethodCount() const {
		return associated_class.getVtableSize() - getParentCount();
	}*/
}
