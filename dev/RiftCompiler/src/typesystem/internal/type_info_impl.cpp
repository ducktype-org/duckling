#include "type_info_impl.hpp"
#include <queue>

namespace ts::internal {
	/**
	 * \brief Gets the global TypeInfoImpl storage structure.
	 * \return The global TypeInfoImpl storage structure.
	 */
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes() {
		static std::vector<base::unique_ptr<const TypeInfoImpl>> type_info_impl_storage{};
		return type_info_impl_storage;
	}

	/**
	 * \brief Gets the maximum size of a type in a vector.
	 * \param types Vector of types to aggregate over.
	 * \return The maximum size of a type in the vector.
	 */
	usize maxTypeVectorSizes(const std::vector<TypeDesc<>>& types) {
		usize max = 0;
		for (const auto& type: types) max = std::max(max, type.getType().getSize());
		return max;
	}

	/**
	 * \brief Creates a human-readable string representation of a vector of types.
	 * \param types Vector of types to stringify.
	 * \return A human-readable string representing a sequence of types.
	 */
	std::string showTypeVector(const std::vector<TypeDesc<>>& types) {
		std::string res = "(";
		for (const auto& t: types) res += t.getType().show() + ",";
		res += ")";

		return res;
	}

	FunctionInfoImpl::FunctionInfoImpl(
			std::vector<TypeDesc<>> parameterTypes, TypeDesc<> resultType, i32 flags
		):
			  TypeInfoImpl(POINTER_SIZE),
			  parameterTypes(std::move(parameterTypes)),
			  resultType(resultType),
			  flags(flags) {
		representation = "Function " + showTypeVector(this->parameterTypes) + " -> ("
					   + resultType.getType().show() + ")";
	}

	usize sumTypeVectorSizes(const std::vector<TypeDesc<>>& types) {
		usize sum = 0;
		for (const auto& type: types) sum += type.getType().getSize();
		return sum;
	}

	TupleInfoImpl::TupleInfoImpl(const std::vector<TypeDesc<>>& tuple_types)
		  // @TODO: Padding (size)?
		  :
		  TypeInfoImpl(sumTypeVectorSizes(tuple_types)),
		  underlyingTypes(tuple_types),
		  offsets(tuple_types.size() + 1, 0) {
		representation = "Tuple" + showTypeVector(tuple_types);
		for (i32 i = 0; i < underlyingTypes.size(); i++)
			offsets[i + 1] = offsets[i] + underlyingTypes[i].getType().getSize();
	}

	std::pair<TypeDesc<>, usize> TupleInfoImpl::getMember(usize index) const {
		return { getType(index), offsets[index] };
	}

	VariantInfoImpl::VariantInfoImpl(const std::vector<TypeDesc<>>& variant_types)
	// 1 byte is for information which type is it. Maybe dynamic size in the future.
		:
		TypeInfoImpl(BYTE_SIZE + maxTypeVectorSizes(variant_types)),
		underlyingTypes(variant_types) {
		representation = "Variant" + showTypeVector(variant_types);
	}

	[[nodiscard]]
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
	}
}
