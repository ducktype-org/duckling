// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "response.hpp"

#include <base/str/str_utils.hpp>

#include <algorithm>
#include <ranges>
#include <string>

namespace vm::api::response {
	namespace {
		/// "`worker` (thread 1)", or just "thread 1" when the function name is unknown.
		std::string describe(const UnjoinedThread& unjoined) {
			std::string thread = base::strConcat("thread ", unjoined.thread_id.asInt());
			if_opt_some(unjoined.function_name, function_name) return base::strConcat(
				"`", function_name, "` (", thread, ")"
			);
			return thread;
		}
	}

	std::string ValidationResult::toString() const {
		std::string report;
		if (!memory_valid) report += "The program leaked memory.\n";
		if (unjoined_program_threads.empty()) return report;

		// The run number is only worth printing when the report had more then one run.
		const bool several_runs = unjoined_program_threads.front().run_number
		                       != unjoined_program_threads.back().run_number;

		auto group_begin = unjoined_program_threads.begin();
		while (group_begin != unjoined_program_threads.end()) {
			const usize run       = group_begin->run_number;
			const auto  group_end = std::ranges::find_if(
                group_begin,
                unjoined_program_threads.end(),
                [run](const UnjoinedThread& unjoined) { return unjoined.run_number != run; }
            );

			report += base::strConcat(
				"The program started ",
				base::strJoin(
					std::ranges::subrange(group_begin, group_end) | std::views::transform(describe),
					", "
				),
				several_runs ? base::strConcat(" in run ", run) : std::string(),
				std::distance(group_begin, group_end) == 1 ? " and never joined it.\n"
														   : " and never joined them.\n"
			);
			group_begin = group_end;
		}
		return report;
	}
}
