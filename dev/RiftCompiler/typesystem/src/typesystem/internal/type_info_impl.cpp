#include "type_info_impl.hpp"
#include <queue>
#include <utility>

namespace ts::internal {
	Kind TypeInfoImpl::staticKind       = Kind::Any;
	Kind UnitInfoImpl::staticKind       = Kind::Unit;
	Kind VoidInfoImpl::staticKind       = Kind::Void;
	Kind ByteInfoImpl::staticKind       = Kind::Byte;
	Kind BoolInfoImpl::staticKind       = Kind::Bool;
	Kind CharInfoImpl::staticKind       = Kind::Char;
	Kind IntegralInfoImpl::staticKind   = Kind::Integral;
	Kind FloatInfoImpl::staticKind      = Kind::Float;
	Kind RawPointerInfoImpl::staticKind = Kind::RawPointer;
	Kind PointerInfoImpl::staticKind    = Kind::Pointer;
	Kind ReferenceInfoImpl::staticKind  = Kind::Reference;
	Kind TupleInfoImpl::staticKind      = Kind::Tuple;
	Kind FunctionInfoImpl::staticKind   = Kind::Function;
	Kind VariantInfoImpl::staticKind    = Kind::Variant;
	Kind NamespaceInfoImpl::staticKind  = Kind::Namespace;
	Kind ModuleInfoImpl::staticKind     = Kind::Module;
	Kind MetaInfoImpl::staticKind       = Kind::Meta;

	/**
	 * @brief Gets the global TypeInfoImpl storage structure.
	 * @return The global TypeInfoImpl storage structure.
	 */
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes() {
		static std::vector<base::unique_ptr<const TypeInfoImpl>> type_info_impl_storage{};
		return type_info_impl_storage;
	}

	/**
	 * @brief Gets the sum of the sizes of the types in a vector.
	 * @param types Vector of types to aggregate over.
	 * @return The total size od the types in the vector.
	 */
	usize sumTypeVectorSizes(const std::vector<ComponentType>& types) {
		usize sum = 0;
		for (const auto& type: types) sum += type.type.getSize();
		return sum;
	}

	/**
	 * @brief Gets the maximum size of a type in a vector.
	 * @param types Vector of types to aggregate over.
	 * @return The maximum size of a type in the vector.
	 */
	usize maxTypeVectorSizes(const std::vector<TypeDesc<>>& types) {
		usize max = 0;
		for (const auto& type: types) max = std::max(max, type.getType().getSize());
		return max;
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
		  TypeInfoImpl(sumTypeVectorSizes(components)),
		  components(std::move(components)) {
		representation = stringifyTypeVector(components);
	}

	FunctionInfoImpl::FunctionInfoImpl(
		std::vector<TypeInfo> parameter_types,
		const TypeInfo        result_type,
		const bool            pure,
		const bool            free
	):
		  TypeInfoImpl((1 + !free) * POINTER_SIZE),
		  parameter_types(std::move(parameter_types)),
		  result_type(result_type),
		  pure(pure),
		  free(free) {
		representation = "Function " + stringifyTypeVector(this->parameter_types) + " -> ("
		               + result_type.toString() + ")";
	}

	VariantInfoImpl::VariantInfoImpl(const std::vector<TypeDesc<>>& variant_types)
		  // 1 byte is for information which type is it. Maybe dynamic size in the future.
		  :
		  TypeInfoImpl(BYTE_SIZE + maxTypeVectorSizes(variant_types)),
		  underlyingTypes(variant_types) {
		representation = "Variant" + stringifyTypeVector(variant_types);
	}

}
