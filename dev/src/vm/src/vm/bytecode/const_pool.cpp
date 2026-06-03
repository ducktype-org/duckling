#include "const_pool.hpp"

#include <base/extend_cpp/variant_match.hpp>

namespace vm::code {
	bool ConstantValue::operator==(const ConstantValue& other) const {
		if (data->index() != other.data->index()) return false;

		variant_match(*data) {
			variant_case(ConstantU64, u64_val) {
				return u64_val == std::get<ConstantU64>(*other.data);
			}
			variant_case(ConstantClass, struct_val) {
				return struct_val == std::get<ConstantClass>(*other.data);
			}
			variant_case(ConstantFixedSizeTable, array_val) {
				return array_val == std::get<ConstantFixedSizeTable>(*other.data);
			}
		}

		return false;
	}
}  // namespace vm::code
