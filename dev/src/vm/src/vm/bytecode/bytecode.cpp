#include "bytecode.hpp"

namespace vm::code {

	void CodeCollection::mergeFrom(CodeCollection&& other) {
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
		constants.insert(
			constants.end(),
			std::make_move_iterator(other.constants.begin()),
			std::make_move_iterator(other.constants.end())
		);
		auto _ = std::move(other);
	}

}  // namespace vm::code
