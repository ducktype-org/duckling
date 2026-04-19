#include "bytecode.hpp"

#include <base/config/build_type.hpp>
#include <base/except/exceptions.hpp>

#include <algorithm>

namespace vm::code {

	void CodeCollection::mergeFrom(CodeCollection&& other) {
		IF_BUILD_TYPE_DEV({
			for (const auto& func : other.functions) {
				CORE_ASSERT(
					std::ranges::none_of(
						functions, [&](const auto& f) { return f.name.str == func.name.str; }
					),
					"Duplicate function during DVM module merge: ",
					func.name.str
				);
			}
			for (const auto& global : other.global_data) {
				CORE_ASSERT(
					std::ranges::none_of(
						global_data,
						[&](const auto& g) { return g.name.str == global.name.str; }
					),
					"Duplicate global during DVM module merge: ",
					global.name.str
				);
			}
		});

		functions.insert(
			functions.end(),
			std::make_move_iterator(other.functions.begin()),
			std::make_move_iterator(other.functions.end())
		);
		types.insert(
			types.end(),
			std::make_move_iterator(other.types.begin()),
			std::make_move_iterator(other.types.end())
		);
		global_data.insert(
			global_data.end(),
			std::make_move_iterator(other.global_data.begin()),
			std::make_move_iterator(other.global_data.end())
		);
		external_c_functions.insert(
			external_c_functions.end(),
			std::make_move_iterator(other.external_c_functions.begin()),
			std::make_move_iterator(other.external_c_functions.end())
		);
	}

}  // namespace vm::code
