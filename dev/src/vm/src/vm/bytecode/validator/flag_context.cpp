#include "flag_context.hpp"

#include <vm/bytecode/validator/errors.hpp>

#include <queue>
#include <unordered_set>
#include <vector>

namespace vm::code {
	std::unordered_set<base::StrID> getNewFunctionNames(const std::vector<Function>& new_functions) {
		std::unordered_set<base::StrID> result;
		for (const auto& func: new_functions) result.insert(func.name);
		return result;
	}

	base::HashMap<base::StrID, std::vector<base::StrID>> getCallGraph(
		const std::vector<Function>& new_functions
	) {
		base::HashMap<base::StrID, std::vector<base::StrID>> result;

		for (const auto& func: new_functions) {
			std::vector<base::StrID> called_funcs;
			for (const auto& instruction: func.body) {
				instr_match(instruction) {
					instr_case(instructions::Op_call_func, call_func) {
						called_funcs.push_back(call_func.function.function_name);
					}
					// A tail-call is a direct call to a named function, so its callee's flags must
					// propagate to the caller just like a regular call.
					instr_case(instructions::Op_ret_tailcall_func, tailcall) {
						called_funcs.push_back(tailcall.function.function_name);
					}
					// set_threadctx names the function a spawned thread will run; its effects are
					// caused by the spawner, so it is an edge for flag propagation too.
					instr_case(instructions::Op_set_threadctx, threadctx) {
						called_funcs.push_back(threadctx.function.function_name);
					}
					instr_default {
						// Flags of builtins are known on the instruction level.
						// Similarly, flags of extern C functions are assumed at the instruction level.
					}
				}
			}
			result.put(func.name, std::move(called_funcs));
		}

		return result;
	}

	/// Creates a new graph with the edges reversed relative to the provided call graph, i.e. an
	/// edge caller -> callee becomes callee -> caller. This lets flag propagation walk from a
	/// callee up to every (transitive) caller, so a flag found in a callee is attributed to all
	/// functions that may reach it.
	base::HashMap<base::StrID, std::vector<base::StrID>> transposeCallGraph(
		const base::HashMap<base::StrID, std::vector<base::StrID>>& call_graph
	) {
		base::HashMap<base::StrID, std::vector<base::StrID>> result;

		for (const auto& [caller, callees]: call_graph) {
			if (!result.contains(caller)) result.put(caller, {});
			for (const auto& callee: callees)
				if (result.contains(callee))
					result[callee].push_back(caller);
				else
					result.put(callee, { caller });
		}

		return result;
	}

	InstructionFlag getFlagsForFunction(
		const Function&                        func,
		const ObjIdNameMap<GlobalData>&        globals,
		const ObjIdNameMap<ExternalCFunction>& ext_c_functions
	) {
		InstructionFlag result;
		for (const auto& instruction: func.body)
			result |= getFlagsForInstruction(instruction, globals, ext_c_functions);
		return result;
	}

	// @TODO: #3068 Extend ExecutionConfig and check more flags.
	static void verifyFlagsAgainstConfig(
		const InstructionFlag flags, api::ExecutionConfig config, const base::StrID func_name
	) {
		if (config.no_io) {
			if (flags.contains(InstructionFlagOptions::IORead)
			    || flags.contains(InstructionFlagOptions::IOWrite))
				throw ExecutionConfigViolationError(
					func_name, "no_io flag is set, but function performs I/O"
				);
		}
		if (config.read_only) {
			if (flags.contains(InstructionFlagOptions::GlobalWrite))
				throw ExecutionConfigViolationError(
					func_name, "read_only flag is set, but function modifies global state"
				);
		}
		if (config.single_thread) {
			if (flags.contains(InstructionFlagOptions::Multithread))
				throw ExecutionConfigViolationError(
					func_name, "single_thread flag is set, but function uses multithreading"
				);
		}
	}

	void FlagContext::insertAndValidate(
		const std::vector<Function>&           new_functions,
		const ObjIdNameMap<GlobalData>&        globals,
		const ObjIdNameMap<ExternalCFunction>& ext_c_functions,
		api::ExecutionConfig                   config
	) {
		// The validation proceeds as follows:
		// 1. A graph of function calls is built, containing:
		//    - all new functions, and
		//    - all old functions which are called by some new functions
		// 2. The graph is transposed, to facilitate flag propagation
		// 3. For each flag:
		//    a. "Starting" functions are marked, where a "starting" function is an old function
		//       which had that flag, or a new function which satisfies an appropriate condition —
		//       typically that it contains a non-call instruction with the flag.
		//    b. The "marks" are propagated with a BFS algorithm.

		// Graph construction
		const auto new_function_names   = getNewFunctionNames(new_functions);
		const auto call_graph           = getCallGraph(new_functions);
		const auto transpose_call_graph = transposeCallGraph(call_graph);

		// Computing direct flags, i.e. flags without propagation
		base::HashMap<base::StrID, InstructionFlag> direct_flags;
		for (const auto& new_func: new_functions) {
			direct_flags.put(
				new_func.name.str, getFlagsForFunction(new_func, globals, ext_c_functions)
			);
		}
		for (const auto& [func_name, _]: transpose_call_graph)
			if (!new_function_names.contains(func_name))
				direct_flags.put(func_name, flags_in_functions.at(func_name));

		// Prepare helper result structure
		base::HashMap<base::StrID, InstructionFlag> flags_in_new_functions;
		for (const auto& new_func: new_function_names) flags_in_new_functions.put(new_func, {});

		// Iterate over flags to propagate them
		constexpr auto FLAG_COUNT = u32(InstructionFlagOptions::COUNT);
		for (usize flag_id = 0; flag_id < FLAG_COUNT; flag_id++) {
			const auto flag_option = InstructionFlagOptions(flag_id);

			// Mark starting functions
			std::unordered_set<base::StrID> marked_functions;
			for (const auto& [func, _]: transpose_call_graph)
				if (direct_flags.at(func).contains(flag_option)) marked_functions.insert(func);

			// Propagate via BFS
			std::queue<base::StrID> to_visit;
			for (const auto& func: marked_functions) {
				to_visit.push(func);
				marked_functions.insert(func);
			}
			while (!to_visit.empty()) {
				auto current = to_visit.front();
				to_visit.pop();

				for (const auto& next: transpose_call_graph.at(current))
					if (!marked_functions.contains(next)) {
						to_visit.push(next);
						marked_functions.insert(current);
					}
			}

			// Append the flag to all marked new-functions
			for (const auto& func: marked_functions)
				if (new_function_names.contains(func)) flags_in_new_functions[func] |= flag_option;
		}

		// Finally, save the result
		for (const auto& [new_func, flags]: flags_in_new_functions) {
			flags_in_functions.put(new_func, flags);

			verifyFlagsAgainstConfig(flags, config, new_func);
		}
	}
}
