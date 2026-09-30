#include "common.hpp"

namespace vm::debugger::cli::common {
	std::string strip(std::string& string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	std::vector<std::string> extractPrimitiveValues(const vm::api::ProcStatus& status) {
		std::vector<std::string> values;

		if (v_matches(status, vm::api::ExecutionCompleted)) {
			const auto& exit_value = std::get<vm::api::ExecutionCompleted>(status).exit_value;
			if (v_matches(exit_value, std::vector<Ref<vm::IVMValue>>)) {
				for (auto val: std::get<std::vector<Ref<vm::IVMValue>>>(exit_value)) {
					if_opt_some(val->readData(), data) {
						variant_match(data) {
							variant_case(vm::interpreted_data_variant::Primitive, primitive) {
								values.push_back(std::to_string(primitive.value));
							}
						}
					}
				}
			}
		}

		return values;
	}

	std::string withoutControlSequences(const std::string& original) {
		std::string result;
		result.reserve(original.size());

		for (std::size_t i = 0; i < original.size(); ++i)
			if (original[i] == '\x1B' && i + 1 < original.size() && original[i + 1] == '[')
				for (i += 2; i < original.size() && !(original[i] >= '@' && original[i] <= '~');
				     ++i);
			else
				result += original[i];

		return result;
	}
}
