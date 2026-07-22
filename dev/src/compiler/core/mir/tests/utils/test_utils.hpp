#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <helios/queries/queries.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_lowering/mir_unit.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <base/except/exceptions.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace compiler::mir::test_utils {
	inline CRef<mir::Function> getMIRFunctionByName(
		frontend::ModuleID module_id, std::string_view name
	) {
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

	/**
	 * @brief Helper function that check for MIR compilation
	 * errors in a module with given content. Requires that the HELIOS step passes.
	 *
	 * It creates a virtual file from the `module_content` argument
	 * and creates a module tree from it every function call.

	 * @param module_content The content of the module main source file.
	 * @param present_phrases List of phrases that should be present in the logged errors.
	 * @param logged_msg_count Expected number of logged error messages.
	 */
	inline void checkForErrorOnCompileModule(
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

	/**
	 * @brief Helper that verifies a module compiles through MIR without errors.
	 *
	 * Mirrors checkForErrorOnCompileModule but asserts success instead of failure.
	 * Used to verify that previously-failing scenarios now compile.
	 */
	inline void checkForNoErrorOnCompileModule(std::string_view module_content) {
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
			CORE_ASSERT(mir_result.hasValue(), "Expected MIR lowering to succeed, but it failed.");
			logger = query::Context::dumpToOneLoggerAndClear();
			if (logger->hasErrors()) {
				std::stringstream logged_messages;
				logger->terminalPrint(logged_messages);
				std::cerr << "Unexpected logged messages:\n" << logged_messages.str() << "\n";
			}
			CORE_ASSERT(!logger->hasErrors(), "Expected no errors to be logged by MIR.");
		});
	}
}
