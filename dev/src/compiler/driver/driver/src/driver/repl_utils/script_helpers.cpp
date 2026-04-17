#include "script_helpers.hpp"

#include <base/str/str_utils.hpp>

namespace compiler::repl {
	std::expected<ScriptExecutableCall, std::string> getExecutableCallMetadata(
		const vm::code::CodeCollection& chunk, std::string_view wrapper_func_name
	) {
		// Statement wrappers compile to exactly one function per chunk
		if (chunk.functions.size() != 1) {
			return std::unexpected(
				base::strConcat(
					"Expected exactly 1 function in wrapper chunk for '",
					wrapper_func_name,
					"', got ",
					chunk.functions.size()
				)
			);
		}

		const auto& function           = chunk.functions[0];
		auto        wrapper_name_strid = base::StrID(std::string(wrapper_func_name));

		if (function.name.str != wrapper_name_strid) {
			return std::unexpected(
				base::strConcat(
					"Compiled wrapper name mismatch. Expected '",
					wrapper_func_name,
					"', got '",
					function.name.str.strView(),
					"'"
				)
			);
		}

		return ScriptExecutableCall{
			.function_name    = std::string(wrapper_func_name),
			.result_type_name = function.signature.result_types.at(0).str,
		};
	}

	vm::code::Function makeScriptMainFunction(const std::vector<ScriptExecutableCall>& calls) {
		using namespace vm;
		using namespace vm::code;
		using namespace vm::code::instructions;

		// Program execution path is vm::api::run -> VMProcess::doRequest(request::Run)
		// -> runFunction("main", ...), so scripts must synthesize a callable "main" entry.
		// The bytecode validator also enforces that "main" returns i64.
		Function script_main;
		script_main.name                   = Identifier(base::StrID("main"));
		script_main.signature.result_types = { Identifier(base::StrID("i64")) };

		for (usize i = 0; i < calls.size(); ++i) {
			const auto& call = calls[i];

			// VM call validation requires return-value storage to be present on the local stack
			// for non-void calls (FunctionValidator::validateCallAndPop checks it explicitly).
			// We mirror backend lowering's call pattern from FunctionLoweringContext::handleCall:
			// allocate temp slot -> call_func -> deinit temp slot.
			if (call.result_type_name != base::StrID("void")) {
				auto tmp_name = base::StrID(base::strConcat("__script_call_tmp_", i).c_str());
				script_main.body.emplace_back(Op_init_lany_type(
					opargs::StackLocalAny(tmp_name), opargs::Type(call.result_type_name)
				));
			}

			script_main.body.emplace_back(
				Op_call_func(opargs::FunctionName(base::StrID(call.function_name.c_str())))
			);

			if (call.result_type_name != base::StrID("void"))
				script_main.body.emplace_back(Op_deinit());
		}

		// In lowered DVM code, function returns are written to a dedicated local named ret_val
		script_main.body.emplace_back(
			Op_mov_l64_imm(opargs::StackLocal64(base::StrID("ret0")), opargs::Immediate(0))
		);
		script_main.body.emplace_back(Op_ret());

		return script_main;
	}

}  // namespace compiler::repl
