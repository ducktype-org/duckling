#include "bytecode.hpp"

#include <base/extend_cpp/vector_utils.hpp>

namespace vm::code {

	void CodeCollection::deduplicate() {
		base::deduplicateBy(functions, [](const Function& func) { return func.name.str.strView(); });
		base::deduplicateBy(external_c_functions, [](const ExternalCFunction& func) {
			return func.name.str.strView();
		});
		base::deduplicateBy(global_data, [](const GlobalData& g) { return g.name.str.strView(); });
		base::deduplicateBy(types, [](const TypeOfData& t) { return typeName(t); });
	}

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
		auto _ = std::move(other);
	}

}  // namespace vm::code
