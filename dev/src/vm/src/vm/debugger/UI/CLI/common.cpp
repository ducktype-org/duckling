#include "common.hpp"

namespace vm::debugger::cli::common {
	std::string strip(const std::string& string) {
		std::size_t first = string.find_first_not_of(" \t\n\r");
		std::size_t last  = string.find_last_not_of(" \t\n\r");

		return first <= string.length() ? string.substr(first, last - first + 1) : "";
	}

	std::vector<std::string> extractPrimitiveValues(const vm::api::ProcStatus& status) {
		std::vector<std::string> values;

		if (v_matches(status, vm::api::ExecutionCompleted)) {
			const auto& exit_value = std::get<vm::api::ExecutionCompleted>(status).exit_value;

			// TODO: use variant match case
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

			if (v_matches(exit_value, i64)) {
				auto val = std::get<i64>(exit_value);
				values.push_back(std::to_string(val));
			}
		}

		return values;
	}

	std::string statusLine(const vm::api::ProcStatus& status) {
		std::stringstream sstr;
		sstr << "New status: " << vm::api::statusName(status);
		for (std::string& value: common::extractPrimitiveValues(status))
			sstr << " (return value = " << value << ")";
		return sstr.str();
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
