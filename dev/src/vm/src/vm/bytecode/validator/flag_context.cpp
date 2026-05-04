#include "flag_context.hpp"

namespace vm::code {
	std::set<base::StrID> getNewFunctionNames(const std::vector<Function>& new_functions) {
		std::set<base::StrID> result;
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

	void FlagContext::insertAndValidate(
		const std::vector<Function>&           new_functions,
		const ObjIdNameMap<GlobalData>&        globals,
		const ObjIdNameMap<ExternalCFunction>& ext_c_functions
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
			std::set<base::StrID> marked_functions;
			for (const auto& [func, _]: transpose_call_graph)
				if (direct_flags.at(func).contains(flag_option)) marked_functions.insert(func);

			// Propagate via BFS
			std::queue<base::StrID> to_visit;
			for (const auto& func: marked_functions) to_visit.push(func);
			while (!to_visit.empty()) {
				auto current = to_visit.front();
				to_visit.pop();
				marked_functions.insert(current);

				for (const auto& next: transpose_call_graph.at(current))
					if (!marked_functions.contains(next)) to_visit.push(next);
			}

			// Append the flag to all marked new-functions
			for (const auto& func: marked_functions)
				if (new_function_names.contains(func)) flags_in_new_functions[func] |= flag_option;
		}

		// Finally, save the result
		for (const auto& [new_func, flags]: flags_in_new_functions)
			flags_in_functions.put(new_func, flags);
	}
}
