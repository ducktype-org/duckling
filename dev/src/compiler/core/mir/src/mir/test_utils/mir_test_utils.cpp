#include "mir_test_utils.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/queries.hpp>
#include <mir/mir_lowering/mir_queries.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/entry/with_context_do.hpp>

#include <algorithm>
#include <iostream>
#include <sstream>

namespace compiler::mir::test_utils {
	CRef<mir::Function> getMIRFunctionByName(frontend::ModuleID module_id, std::string_view name) {
		auto result = query::utils::withContextCompute([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			for (const auto& function: unit.functions) {
				if (function->declaration->original_name.strView() == name) {
					CRef<mir::Function> foo_mir
						= &ctx.query<compiler::mir::LowerToMIRFunction>({ function })->valueOrPanic();
					return foo_mir;
				}
			}
			CORE_PANIC(base::strConcat("Function with name '", name, "' not found in module "));
		});
		return std::any_cast<CRef<mir::Function>>(result);
	}

	std::set<BlockID> reachableBlocks(const mir::Function& function) {
		std::set<BlockID>    reachable;
		std::vector<BlockID> stack{ function.block_order.front() };
		while (not stack.empty()) {
			auto block_id = stack.back();
			stack.pop_back();
			if (not reachable.insert(block_id).second) continue;
			for (auto succ: getTerminatorSuccessors(function.blocks[block_id].terminator))
				stack.push_back(succ);
		}
		return reachable;
	}

	std::vector<std::vector<BlockID>> pathsFromEntry(const mir::Function& function) {
		std::vector<std::vector<BlockID>> paths;

		auto walk = [&](auto& self, std::vector<BlockID>& path) -> void {
			auto successors = getTerminatorSuccessors(function.blocks[path.back()].terminator);
			bool advanced   = false;
			for (auto succ: successors) {
				if (std::ranges::find(path, succ) != path.end()) continue;
				advanced = true;
				path.push_back(succ);
				self(self, path);
				path.pop_back();
			}
			// Either a terminator without successors, or every successor is already on the path.
			if (not advanced) paths.push_back(path);
		};

		std::vector<BlockID> path{ function.block_order.front() };
		walk(walk, path);
		return paths;
	}

	std::set<BlockID> blocksWithOperation(const mir::Function& function, Operation operation) {
		std::set<BlockID> result;
		for (auto block_id: function.block_order)
			for (const auto& instruction: function.blocks[block_id].instructions)
				if (instruction.operation == operation) result.insert(block_id);
		return result;
	}

	u64 countTerminators(const mir::Function& function, Operation operation) {
		u64 count = 0;
		for (auto block_id: function.block_order)
			if (function.blocks[block_id].terminator.operation == operation) count++;
		return count;
	}

	void checkLoweredModule(
		std::string_view                                            module_content,
		const std::function<void(query::Context&, const MIRUnit&)>& check
	) {
		frontend::ModuleID module_id
			= frontend::createModuleTreeFromContents(module_content, "test_package");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto assert_no_errors = [](std::string_view stage) {
				auto logger = query::Context::dumpToOneLoggerAndClear();
				if (not logger->hasErrors()) return;
				std::stringstream logged_messages;
				logger->terminalPrint(logged_messages);
				CORE_PANIC(base::strConcat(
					"Expected no errors to be logged by ", stage, ", got:\n", logged_messages.str()
				));
			};

			// Diagnostics logged by whatever ran before are not ours to report on.
			(void) query::Context::dumpToOneLoggerAndClear();

			auto hout_result = ctx.query<helios::QueryTopLevelEntities>(module_id);
			assert_no_errors("HELIOS");
			CORE_ASSERT(
				hout_result->hasValue(),
				"Expected top-level entities query to succeed for module content."
			);

			auto mir_result = mir::lowerToMIRUnit(ctx, &hout_result->valueOrPanic());
			assert_no_errors("MIR");
			CORE_ASSERT(mir_result.hasValue(), "Expected MIR lowering to succeed.");

			check(ctx, mir_result.valueOrPanic());
		});
	}

	CRef<mir::Function> functionOfUnit(const mir::MIRUnit& unit, std::string_view name) {
		for (const auto& function: unit.mir_functions)
			if (function->name.strView() == name) return function;
		CORE_PANIC(base::strConcat("Function with name '", name, "' not found in the MIR unit."));
	}

	void checkForErrorOnCompileModule(
		std::string_view                     module_content,
		const std::vector<std::string_view>& present_phrases,
		u64                                  logged_msg_count
	) {
		frontend::ModuleID module_id
			= frontend::createModuleTreeFromContents(module_content, "test_package");

		query::utils::withContextDo([&](query::Context& ctx) {
			auto hout_result = ctx.query<helios::QueryTopLevelEntities>(module_id);
			CORE_ASSERT(
				hout_result->hasValue(),
				"Expected top-level entities query to succeed for module content."
			);
			auto logger = query::Context::dumpToOneLoggerAndClear();
			CORE_ASSERT(!logger->hasErrors(), "Expected no errors to be logged by HELIOS.");

			auto mir_result = mir::lowerToMIRUnit(ctx, &hout_result->valueOrPanic());
			CORE_ASSERT(
				mir_result.hasFailed(), "Expected some MIR query to fail for module lowering."
			);
			logger = query::Context::dumpToOneLoggerAndClear();
			CORE_ASSERT(logger->hasErrors(), "Expected errors to be logged by MIR.");

			std::stringstream logged_messages;
			logger->terminalPrint(logged_messages);
			std::cerr << "Logged messages:\n" << logged_messages.str() << "\n";
			auto msg_count = logger->messageCount();
			CORE_ASSERT(
				msg_count,
				logged_msg_count,
				"Expected logged message count to be " + std::to_string(logged_msg_count)
					+ ", but got " + std::to_string(msg_count)
			);
			for (const auto& phrase: present_phrases) {
				std::string logged_str = logged_messages.str();
				CORE_ASSERT(
					logged_str.find(phrase.data()) != std::string::npos,
					"Expected logged messages to contain phrase: " + std::string(phrase)
				);
			}
		});
	}
}
