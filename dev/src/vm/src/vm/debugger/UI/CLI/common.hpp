#pragma once

#include <diagnostic/highlight_positions.hpp>

#include <vm/api/api.hpp>

#include <string>
#include <vector>

namespace vm::debugger::cli::common {
	std::string strip(std::string& string);

	template<typename T>
	std::string typeToString(const T& status) {
		return std::visit(
			[&](auto&& arg) {
				using TT = std::decay_t<decltype(arg)>;
				return TypeParseTraits<TT>::NAME.data();
			},
			status
		);
	}

	std::vector<std::string> extractPrimitiveValues(const vm::api::ProcStatus& status);

	std::string withoutControlSequences(const std::string& original);
}
