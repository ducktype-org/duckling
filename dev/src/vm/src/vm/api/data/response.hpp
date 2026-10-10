// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "status.hpp"
#include "thread_id.hpp"

#include <base/pointers/box.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/source_position.hpp>

#include <vm/core/safe/type_metadata/type.hpp>
#include <vm/core/vmvalue/ivmvalue.hpp>
#include <vm/core/vmvalue/ivmvalueref.hpp>

namespace vm::api {
	namespace response {
		struct Empty final {};

		struct Output final {
			std::string output;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Output, output);
		};

		struct Type final {
			TypeCRef type;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(Type, type);
		};

		struct VMValue final {
			Box<::vm::IVMValue> vm_value;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(VMValue, vm_value);
		};

		struct CodePosition {
			base::StrID                         function_name;
			u64                                 instr_number;
			base::Optional<dia::SourcePosition> source_position;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(CodePosition, function_name, instr_number);
		};

		struct NumberOfCurrentStackFrames final {
			u64 number_of_stack_frames;
		};

		struct StackFrameData final {
			struct FrameVar final {
				u64                          offset = 0;
				base::Optional<base::StrID>  name;
				base::Optional<base::StrID>  type;
				SharedBox<::vm::IVMValueRef> value;
			};

			base::StrID           function_name;
			std::vector<FrameVar> frame_vars;
		};

		struct ThreadIDs final {
			std::vector<ThreadID> thread_ids;
			NLOHMANN_DEFINE_TYPE_INTRUSIVE(ThreadIDs, thread_ids);
		};

		/**
		 * @brief Information about errors found by `api::deinitAndValidate`.
		 */
		struct ValidationResult {
			/// False when a memory leak was found.
			bool memory_valid{ true };

			/// The threads the user program started and never joined.
			std::vector<ThreadID> unjoined_program_threads;

			[[nodiscard]] bool valid() const {
				return memory_valid && unjoined_program_threads.empty();
			}

			NLOHMANN_DEFINE_TYPE_INTRUSIVE(ValidationResult, memory_valid, unjoined_program_threads);
		};
	}

	/**
	 * @brief Human readable report of what a teardown found, for a caller which just wants to
	 * print it.
	 *
	 * @return The report, empty when the program left nothing behind.
	 */
	[[nodiscard]] inline std::string validationToString(const response::ValidationResult& validation
	) {
		std::string report;
		if (!validation.memory_valid) report += "The program leaked memory.\n";

		const std::vector<ThreadID>& unjoined = validation.unjoined_program_threads;
		if (!unjoined.empty()) {
			const bool single = unjoined.size() == 1;
			report += single ? "The program started thread " : "The program started threads ";
			for (usize i = 0; i < unjoined.size(); i++) {
				if (i != 0) report += ", ";
				report += base::toString(unjoined.at(i).asInt());
			}
			report += single ? " and never joined it.\n" : " and never joined them.\n";
		}
		return report;
	}

	using Response = std::variant<
		ProcStatus,
		response::Output,
		response::Type,
		response::Empty,
		response::CodePosition,
		response::VMValue,
		response::ValidationResult,
		ThreadID,
		response::NumberOfCurrentStackFrames,
		response::StackFrameData,
		response::ThreadIDs,
		ExitValue>;
}
