#include "type_info_impl.hpp"
#include <queue>

namespace ts::internal {
	std::vector<base::unique_ptr<const TypeInfoImpl>>& getTypes() {
		static std::vector<base::unique_ptr<const TypeInfoImpl>> type_info_impl_storage{};
		return type_info_impl_storage;
	}

	size_t maxTypeVectorSizes(const std::vector<TypeDesc<>>& types) {
		size_t max = 0;
		for (const auto& type: types) {
			max = std::max(max, type.getType().getSize());
		}
		return max;
	}

	VariantInfoImpl::VariantInfoImpl(const std::vector<TypeDesc<>>& variant_types)
		// 1 byte is for information which type is it. Maybe dynamic size in the future.
		: TypeInfoImpl(BYTE_SIZE + maxTypeVectorSizes(variant_types)),
		  variant_types(variant_types) {
			representation = "Variant" + showVector(variant_types);
		  }

	size_t sumTypeVectorSizes(const std::vector<TypeDesc<>>& types) {
		size_t sum = 0;
		for (const auto& type: types) {
			sum += type.getType().getSize();
		}
		return sum;
	}

	TupleInfoImpl::TupleInfoImpl(const std::vector<TypeDesc<>>& tuple_types)
		// @TODO: Padding (size)?
		: TypeInfoImpl(sumTypeVectorSizes(tuple_types)), underlyingTypes(tuple_types),
		  offsets(tuple_types.size() + 1, 0) {
		representation = "Tuple" + showVector(tuple_types);
		for (int i = 0; i < underlyingTypes.size(); i++) {
			offsets[i + 1] = offsets[i] + underlyingTypes[i].getType().getSize();
		}
	}

	std::pair<TypeDesc<>, size_t> TupleInfoImpl::getMember(size_t index) const {
		return {getType(index), offsets[index]};
	}

	[[nodiscard]] ClassInfo VTableInfoImpl::getAssociatedClass() const {
		return associated_class;
	}
	[[nodiscard]] size_t VTableInfoImpl::getParentCount() const {
		return associated_class.virtualAncestors().size();
	}

	[[nodiscard]] size_t VTableInfoImpl::getMethodCount() const {
		return associated_class.getVtableSize() - getParentCount(); 
	}
}
